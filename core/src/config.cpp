#include "config.hpp"


namespace {
    std::string trimNewlines(std::string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        return s;
    }
}// namespace

std::unique_ptr<Config::Settings> Config::load(RunMode mode, const std::string &filePath) {
    const YAML::Node yaml = YAML::LoadFile(filePath);
    auto s = std::make_unique<Settings>();

    // Deprecated: the token used to live in general.
    const std::string legacyToken = yaml["general"]["token"].as<std::string>("");

    s->general = {yaml["general"]["protocol"].as<std::string>(""),
                  yaml["general"]["bufferSize"].as<unsigned int>(65536),
                  yaml["general"]["fakeUrls"].as<std::vector<std::string>>(),
                  yaml["general"]["methods"].as<std::vector<std::string>>(),
                  yaml["general"]["endPoints"].as<std::vector<std::string>>(),
                  yaml["general"]["timeout"].as<unsigned short>(10),
                  yaml["general"]["pullTimeout"].as<unsigned short>(1),
                  yaml["general"]["tunnelEnable"].as<bool>(false),
                  yaml["general"]["connectionReuse"].as<bool>(false),
                  yaml["general"]["aesEnable"].as<bool>(true),
                  yaml["general"]["tlsEnable"].as<bool>(false),
                  yaml["general"]["tlsVerifyPeer"].as<bool>(false),
                  yaml["general"]["tlsCertFile"].as<std::string>(""),
                  yaml["general"]["tlsKeyFile"].as<std::string>(""),
                  yaml["general"]["tlsCaFile"].as<std::string>("")};
    s->log = {yaml["log"]["logLevel"].as<std::string>("INFO"),
              yaml["log"]["logFile"].as<std::string>()};
    s->server = {yaml["server"]["threads"].as<unsigned short>(2),
                 yaml["server"]["listenIp"].as<std::string>("127.0.0.1"),
                 yaml["server"]["listenPort"].as<unsigned short>(8080)};
    s->agent = {yaml["agent"]["threads"].as<unsigned short>(2),
                yaml["agent"]["listenIp"].as<std::string>("127.0.0.1"),
                yaml["agent"]["listenPort"].as<unsigned short>(8080),
                yaml["agent"]["serverIp"].as<std::string>("127.0.0.1"),
                yaml["agent"]["serverPort"].as<unsigned short>(8080),
                yaml["agent"]["httpVersion"].as<std::string>("1.1"),
                yaml["agent"]["userAgent"].as<std::string>("NipoAgent"),
                yaml["agent"]["token"].as<std::string>("")};

    if (mode == RunMode::agent && s->agent.token.empty() && !legacyToken.empty()) {
        s->agent.token = legacyToken;
        s->warnings.push_back("general.token is deprecated; move it to agent.token");
    }

    if (mode == RunMode::server) {
        const YAML::Node users = yaml["users"];
        if (users && users.IsSequence()) {
            s->usersFromFile = true;
            for (const auto &u: users) {
                const std::string token = u["token"].as<std::string>();
                s->users.push_back({u["name"].as<std::string>(), token,
                                    deriveAesKey(token), u["enabled"].as<bool>(true)});
            }
        } else {
            // No users list: accept the deprecated general.token as one user.
            s->users.push_back({"default", legacyToken, deriveAesKey(legacyToken), true});
            if (!legacyToken.empty()) {
                s->warnings.push_back(
                        "general.token is deprecated on the server; "
                        "put the token in a users list instead");
            }
        }
    }

    return s;
}

Config::Config(RunMode mode, const std::string &filePath)
    : runMode_(mode),
      filePath_(filePath),
      threads_(0),
      listenIp_("127.0.0.1"),
      listenPort_(0) {
    publish(load(mode, filePath));

    const Settings &s = current();
    switch (runMode_) {
        case RunMode::server:
            threads_ = s.server.threads;
            listenIp_ = s.server.listenIp;
            listenPort_ = s.server.listenPort;
            break;

        case RunMode::agent:
            threads_ = s.agent.threads;
            listenIp_ = s.agent.listenIp;
            listenPort_ = s.agent.listenPort;
            break;
    }
}

Config::~Config() = default;

const Config::Settings &Config::current() const {
    return *current_.load(std::memory_order_acquire);
}

void Config::publish(std::unique_ptr<Settings> settings) {
    std::lock_guard<std::mutex> lock(configMutex_);
    snapshots_.push_back(std::move(settings));
    current_.store(snapshots_.back().get(), std::memory_order_release);
}

Config::ReloadResult Config::reload() {
    std::lock_guard<std::mutex> lock(reloadMutex_);

    const BoolStr valid = validateConfigFile(modeToString(), filePath_);
    if (!valid.ok) {
        return {false, "config not reloaded: " + trimNewlines(valid.message), {}};
    }

    std::unique_ptr<Settings> next;
    try {
        next = load(runMode_, filePath_);
    } catch (const std::exception &e) {
        return {false, std::string("config not reloaded: ") + e.what(), {}};
    }

    const Settings &prev = current();
    std::vector<std::string> notes;

    // Keep settings that are only applied at startup, and say so.
    auto keep = [&notes](auto &field, const auto &old, const char *name) {
        if (field != old) {
            notes.push_back(std::string(name) + " changed but needs a restart");
            field = old;
        }
    };
    if (runMode_ == RunMode::server) {
        keep(next->server.listenIp, prev.server.listenIp, "server.listenIp");
        keep(next->server.listenPort, prev.server.listenPort, "server.listenPort");
        keep(next->server.threads, prev.server.threads, "server.threads");
    } else {
        keep(next->agent.listenIp, prev.agent.listenIp, "agent.listenIp");
        keep(next->agent.listenPort, prev.agent.listenPort, "agent.listenPort");
        keep(next->agent.threads, prev.agent.threads, "agent.threads");
    }
    // Open connections are bound to TLS or plain TCP at accept time.
    keep(next->general.tlsEnable, prev.general.tlsEnable, "general.tlsEnable");

    // A user loses access when they disappear, are disabled, or get a new token.
    std::vector<std::string> revoked;
    for (const auto &old: prev.users) {
        if (!old.enabled) continue;
        bool stillValid = false;
        for (const auto &u: next->users) {
            if (u.name == old.name) {
                stillValid = u.enabled && u.token == old.token;
                break;
            }
        }
        if (!stillValid) revoked.push_back(old.name);
    }

    std::size_t enabled = 0;
    for (const auto &u: next->users) {
        if (u.enabled) ++enabled;
    }

    std::string message = "config reloaded";
    if (runMode_ == RunMode::server) {
        message += "; " + std::to_string(enabled) + " of " +
                   std::to_string(next->users.size()) + " users enabled";
        if (!revoked.empty()) {
            message += "; access revoked for:";
            for (const auto &name: revoked) message += " " + name;
        }
    }
    for (const auto &note: notes) message += "; " + note;

    publish(std::move(next));
    return {true, message, revoked};
}

std::string Config::toString() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    const Settings &cur = current();
    const General &general_ = cur.general;
    const Log &log_ = cur.log;
    const Server &server_ = cur.server;
    const Agent &agent_ = cur.agent;

    std::stringstream ss;

    ss << "\nConfig :\n"
       << " General :\n"
       << "   protocol: " << general_.protocol << "\n"
       << "   bufferSize: " << general_.bufferSize << "\n"
       << "   fakeUrls: ";

    for (std::size_t i = 0; i < general_.fakeUrls.size(); ++i) {
        ss << general_.fakeUrls[i];

        if (i != general_.fakeUrls.size() - 1) {
            ss << ", ";
        }
    }

    ss << "\n"
       << "   methods: ";

    for (std::size_t i = 0; i < general_.methods.size(); ++i) {
        ss << general_.methods[i];

        if (i != general_.methods.size() - 1) {
            ss << ", ";
        }
    }

    ss << "\n"
       << "   endPoints: ";

    for (std::size_t i = 0; i < general_.endPoints.size(); ++i) {
        ss << general_.endPoints[i];

        if (i != general_.endPoints.size() - 1) {
            ss << ", ";
        }
    }

    ss << "\n"
       << "   timeout: " << general_.timeout << "\n"
       << "   pullTimeout: " << general_.pullTimeout << "\n"
       << "   tunnelEnable: " << std::boolalpha << general_.tunnelEnable << "\n"
       << "   connectionReuse: " << std::boolalpha << general_.connectionReuse << "\n"
       << "   aesEnable: " << std::boolalpha << general_.aesEnable << "\n"
       << "   tlsEnable: " << std::boolalpha << general_.tlsEnable << "\n"
       << "   tlsVerifyPeer: " << std::boolalpha << general_.tlsVerifyPeer << "\n"
       << "   tlsCertFile: " << general_.tlsCertFile << "\n"
       << "   tlsKeyFile: " << general_.tlsKeyFile << "\n"
       << "   tlsCaFile: " << general_.tlsCaFile << "\n"

       << " Log :\n"
       << "   logLevel: " << log_.level << "\n"
       << "   logFile: " << log_.file << "\n"

       << " Server :\n"
       << "   threads: " << server_.threads << "\n"
       << "   listenIp: " << server_.listenIp << "\n"
       << "   listenPort: " << server_.listenPort << "\n"

       << " Agent :\n"
       << "   threads: " << agent_.threads << "\n"
       << "   listenIp: " << agent_.listenIp << "\n"
       << "   listenPort: " << agent_.listenPort << "\n"
       << "   serverIp: " << agent_.serverIp << "\n"
       << "   serverPort: " << agent_.serverPort << "\n"
       << "   httpVersion: " << agent_.httpVersion << "\n"
       << "   userAgent: " << agent_.userAgent << "\n"
       << "   token: " << (agent_.token.empty() ? "" : "<hidden>") << "\n";

    if (runMode_ == RunMode::server && cur.usersFromFile) {
        ss << " Users :\n";
        for (const auto &u: cur.users) {
            ss << "   " << u.name << (u.enabled ? "" : " (disabled)") << "\n";
        }
    }

    return ss.str();
}

const Config::General &Config::general() const {
    return current().general;
}

const Config::Log &Config::log() const {
    return current().log;
}

const Config::Server &Config::server() const {
    return current().server;
}

const Config::Agent &Config::agent() const {
    return current().agent;
}

const std::vector<User> &Config::users() const {
    return current().users;
}

const std::vector<std::string> &Config::warnings() const {
    return current().warnings;
}

unsigned short Config::threads() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return threads_;
}

void Config::threads(unsigned short threads) {
    std::lock_guard<std::mutex> lock(configMutex_);
    threads_ = threads;
}

const std::string &Config::listenIp() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenIp_;
}

void Config::listenIp(const std::string &ip) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenIp_ = ip;
}

unsigned short Config::listenPort() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenPort_;
}

void Config::listenPort(unsigned short port) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenPort_ = port;
}

RunMode Config::runMode() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return runMode_;
}

const std::string &Config::filePath() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return filePath_;
}

std::string Config::modeToString() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    switch (runMode_) {
        case RunMode::server:
            return "server";
        case RunMode::agent:
            return "agent";
        default:
            return "UNKNOWN MODE";
    }
}

std::string Config::randomFakeUrl() const {
    const General &general_ = current().general;

    if (general_.fakeUrls.empty()) {
        return "www.google.com";
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<std::size_t> distribution(
            0,
            general_.fakeUrls.size() - 1);

    return general_.fakeUrls[distribution(generator)];
}

std::string Config::randomMethod() const {
    const General &general_ = current().general;

    if (general_.methods.empty()) {
        return "GET";
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<std::size_t> distribution(
            0,
            general_.methods.size() - 1);

    return general_.methods[distribution(generator)];
}

std::string Config::randomEndPoint() const {
    const General &general_ = current().general;

    if (general_.endPoints.empty()) {
        return "api";
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<std::size_t> distribution(
            0,
            general_.endPoints.size() - 1);

    return general_.endPoints[distribution(generator)];
}