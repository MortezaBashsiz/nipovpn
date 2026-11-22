#pragma once

#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <yaml-cpp/yaml.h>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

static constexpr std::size_t BufferSize = 65536;
static constexpr std::size_t MinTokenLength = 32;
static constexpr const char *SampleToken = "af445adb-2434-4975-9445-2c1b2231";

class Uncopyable {
public:
    Uncopyable() = default;

private:
    Uncopyable(const Uncopyable &) = delete;
    Uncopyable &operator=(const Uncopyable &) = delete;
};

inline void OUT(const auto &message) {
    std::cout << "OUT OUT OUT OUT OUT OUT OUT OUT : " << message
              << std::endl;
}

struct BoolStr {
    bool ok;
    std::string message;
};

inline bool fileExists(const std::string &name) {
    std::ifstream file(name.c_str());
    return file.good();
}

inline std::string streambufToString(const boost::asio::streambuf &buff) {
    return {boost::asio::buffers_begin(buff.data()),
            boost::asio::buffers_begin(buff.data()) + buff.size()};
}

inline void copyStringToStreambuf(const std::string &inputStr,
                                  boost::asio::streambuf &buff) {
    buff.consume(buff.size());
    std::ostream os(&buff);
    os << inputStr;
}

inline void moveStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
    source.consume(source.size());
}

inline void copyStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
}

inline std::string hexArrToStr(const unsigned char *data, std::size_t size) {
    std::stringstream tempStr;
    tempStr << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < size; ++i) {
        tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
    }

    return tempStr.str();
}

inline unsigned int hexToInt(const std::string &hexString) {
    unsigned int result{};
    std::stringstream hexStr(hexString);
    hexStr >> std::hex >> result;
    return result;
}

inline std::string hexToASCII(const std::string &hex) {
    std::string ascii;

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string part = hex.substr(i, 2);
        char ch = static_cast<char>(stoul(part, nullptr, 16));
        ascii += ch;
    }

    return ascii;
}

inline unsigned char charToHex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return 0;
}

inline std::vector<unsigned char> strToHexArr(const std::string &hexStr) {
    std::vector<unsigned char> result(hexStr.size() / 2);

    for (std::size_t i = 0; i < hexStr.size() / 2; ++i) {
        result[i] = 16 * charToHex(hexStr[2 * i]) + charToHex(hexStr[2 * i + 1]);
    }

    return result;
}

inline std::string hexStreambufToStr(const boost::asio::streambuf &buff) {
    return hexArrToStr(
            reinterpret_cast<const unsigned char *>(streambufToString(buff).c_str()),
            buff.size());
}

inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &delimiter) {
    std::vector<std::string> result;
    std::string tempStr = str;

    while (!tempStr.empty()) {
        size_t index = tempStr.find(delimiter);

        if (index != std::string::npos) {
            result.push_back(tempStr.substr(0, index));
            tempStr = tempStr.substr(index + delimiter.size());

            if (tempStr.empty()) {
                result.push_back(tempStr);
            }
        } else {
            result.push_back(tempStr);
            tempStr.clear();
        }
    }

    return result;
}

inline std::string decode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    std::string temp = inputStr;
    boost::algorithm::trim_right_if(temp, boost::is_any_of("="));

    try {
        using binaryFromBase64 = binary_from_base64<std::string::const_iterator>;
        using transformWidth = transform_width<binaryFromBase64, 8, 6>;

        return {transformWidth(temp.begin()), transformWidth(temp.end())};
    } catch (const std::exception &) {
        throw std::runtime_error("Invalid Base64 encoding");
    }
}

inline std::string encode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    using transformWidth = transform_width<std::string::const_iterator, 6, 8>;
    using base64Enc = base64_from_binary<transformWidth>;

    std::string encoded(base64Enc(inputStr.begin()), base64Enc(inputStr.end()));

    std::size_t numPadChars = (3 - inputStr.size() % 3) % 3;
    encoded.append(numPadChars, '=');

    return encoded;
}

static constexpr std::size_t AesGcmNonceLength = 12;
static constexpr std::size_t AesGcmTagLength = 16;

inline std::string deriveAesKey(const std::string &token) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    EVP_Digest(token.data(), token.size(), digest, &digestLen, EVP_sha256(), NULL);
    return std::string(reinterpret_cast<char *>(digest), digestLen);
}

inline BoolStr aes256EncryptWithKey(const std::string &plaintext,
                                    const std::string &key) {
    BoolStr result{false, "Encryption failed"};

    unsigned char nonce[AesGcmNonceLength];
    if (!RAND_bytes(nonce, sizeof(nonce))) {
        result.message = "Nonce generation failed";
        return result;
    }

    std::vector<unsigned char> ciphertext(plaintext.size());

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, AesGcmNonceLength, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), NULL, NULL,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           nonce) != 1) {
        result.message = "Encryption initialization failed";
        return result;
    }

    int len = 0;
    if (EVP_EncryptUpdate(
                ctx.get(), ciphertext.data(), &len,
                reinterpret_cast<const unsigned char *>(plaintext.data()),
                plaintext.size()) != 1) {
        result.message = "Encryption update failed";
        return result;
    }
    int ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
        result.message = "Encryption finalization failed";
        return result;
    }
    ciphertext_len += len;

    unsigned char tag[AesGcmTagLength];
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, AesGcmTagLength, tag) != 1) {
        result.message = "Tag generation failed";
        return result;
    }

    std::string final_result(reinterpret_cast<char *>(nonce), sizeof(nonce));
    final_result.append(reinterpret_cast<char *>(ciphertext.data()), ciphertext_len);
    final_result.append(reinterpret_cast<char *>(tag), sizeof(tag));

    result.ok = true;
    result.message = final_result;
    return result;
}

inline BoolStr aes256DecryptWithKey(const std::string &sealed,
                                    const std::string &key) {
    BoolStr result{false, "Decryption failed"};

    if (sealed.size() < AesGcmNonceLength + AesGcmTagLength) {
        result.message = "Ciphertext too short";
        return result;
    }

    const auto *nonce = reinterpret_cast<const unsigned char *>(sealed.data());
    const auto *ciphertext = nonce + AesGcmNonceLength;
    const std::size_t ciphertextLen = sealed.size() - AesGcmNonceLength - AesGcmTagLength;
    std::string tag = sealed.substr(sealed.size() - AesGcmTagLength);

    std::vector<unsigned char> plaintext(ciphertextLen);

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, AesGcmNonceLength, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), NULL, NULL,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           nonce) != 1) {
        result.message = "Decryption initialization failed";
        return result;
    }

    int len = 0;
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext,
                          ciphertextLen) != 1) {
        result.message = "Decryption update failed";
        return result;
    }
    int plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, AesGcmTagLength,
                            tag.data()) != 1) {
        result.message = "Setting tag failed";
        return result;
    }

    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
        result.message = "Authentication failed (wrong token or tampered payload)";
        return result;
    }
    plaintext_len += len;

    result.ok = true;
    result.message =
            std::string(reinterpret_cast<char *>(plaintext.data()), plaintext_len);

    return result;
}

constexpr std::size_t HmacTagLength = 32;

inline std::string hmacSha256(const std::string &data, const std::string &key) {
    unsigned char tag[EVP_MAX_MD_SIZE];
    unsigned int tagLen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char *>(data.data()), data.size(), tag, &tagLen);
    return std::string(reinterpret_cast<char *>(tag), tagLen);
}

inline std::string signWithKey(const std::string &payload, const std::string &key) {
    return payload + hmacSha256(payload, key);
}

inline BoolStr verifyWithKey(const std::string &signedPayload, const std::string &key) {
    if (signedPayload.size() < HmacTagLength) return {false, "payload too short for a signature"};
    const std::string payload = signedPayload.substr(0, signedPayload.size() - HmacTagLength);
    const std::string tag = hmacSha256(payload, key);
    if (CRYPTO_memcmp(tag.data(), signedPayload.data() + payload.size(), HmacTagLength) != 0) {
        return {false, "signature does not match"};
    }
    return {true, payload};
}

inline BoolStr aes256Encrypt(const std::string &plaintext,
                             const std::string &token) {
    return aes256EncryptWithKey(plaintext, deriveAesKey(token));
}

inline BoolStr aes256Decrypt(const std::string &sealed,
                             const std::string &token) {
    return aes256DecryptWithKey(sealed, deriveAesKey(token));
}

inline std::string makeEnvelope(const std::string &action,
                                const std::string &session,
                                const std::string &data) {
    return action + "\n" + session + "\n" + data;
}

struct Envelope {
    std::string action;
    std::string session;
    std::string data;
};

inline bool parseEnvelope(const std::string &raw, Envelope &out) {
    const auto actionEnd = raw.find('\n');
    if (actionEnd == std::string::npos) return false;

    const auto sessionEnd = raw.find('\n', actionEnd + 1);
    if (sessionEnd == std::string::npos) return false;

    out.action = raw.substr(0, actionEnd);
    out.session = raw.substr(actionEnd + 1, sessionEnd - actionEnd - 1);
    out.data = raw.substr(sessionEnd + 1);
    return true;
}

inline bool parseByteSize(const std::string &text, std::uint64_t &out) {
    std::size_t i = 0;
    while (i < text.size() && text[i] == ' ') ++i;
    const std::size_t numStart = i;
    bool dot = false;
    while (i < text.size() && ((text[i] >= '0' && text[i] <= '9') || (text[i] == '.' && !dot))) {
        if (text[i] == '.') dot = true;
        ++i;
    }
    if (i == numStart) return false;
    double value = 0;
    try {
        value = std::stod(text.substr(numStart, i - numStart));
    } catch (const std::exception &) {
        return false;
    }
    while (i < text.size() && text[i] == ' ') ++i;
    std::string unit = text.substr(i);
    while (!unit.empty() && unit.back() == ' ') unit.pop_back();
    for (auto &c: unit) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    double multiplier = 0;
    if (unit == "b") multiplier = 1;
    else if (unit == "kb")
        multiplier = 1e3;
    else if (unit == "mb")
        multiplier = 1e6;
    else if (unit == "gb")
        multiplier = 1e9;
    else if (unit == "tb")
        multiplier = 1e12;
    else if (unit == "kib")
        multiplier = 1024.0;
    else if (unit == "mib")
        multiplier = 1024.0 * 1024;
    else if (unit == "gib")
        multiplier = 1024.0 * 1024 * 1024;
    else if (unit == "tib")
        multiplier = 1024.0 * 1024 * 1024 * 1024;
    else
        return false;

    const double bytes = value * multiplier;
    if (bytes < 1 || bytes > 1.8e19) return false;
    out = static_cast<std::uint64_t>(bytes);
    return true;
}

inline std::string formatBytes(std::uint64_t bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1000 && unit < 5) {
        value /= 1000;
        ++unit;
    }
    std::ostringstream out;
    if (unit == 0) out << bytes << " B";
    else
        out << std::fixed << std::setprecision(2) << value << " " << units[unit];
    return out.str();
}

inline bool isValidUserName(const std::string &name) {
    if (name.empty() || name.size() > 64) return false;
    for (const char c: name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                        c == '-' || c == '@';
        if (!ok) return false;
    }
    return true;
}

inline YAML::Node movedSetting(const YAML::Node &yaml, const char *block, const char *key) {
    if (yaml[block][key]) return yaml[block][key];
    return yaml["general"][key];
}

inline bool isDeprecatedSetting(const YAML::Node &yaml, const char *block, const char *key) {
    return !yaml[block][key] && yaml["general"][key];
}

inline BoolStr validateConfigFile(const std::string &mode, const std::string &path) {
    BoolStr result{false, "Validation failed"};

    if (mode != "server" && mode != "agent") {
        result.message = "First argument must be one of [server|agent]\n";
        return result;
    }

    if (!fileExists(path)) {
        result.message = "Config file " + path + " does not exist\n";
        return result;
    }

    YAML::Node configYaml;
    try {
        configYaml = YAML::LoadFile(path);
    } catch (const std::exception &e) {
        result.message =
                std::string("Error parsing config file: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["general"]["token"].as<std::string>("");
        configYaml["general"]["timeout"].as<unsigned short>();
        configYaml["general"]["tunnelEnable"].as<bool>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'general' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["log"]["logFile"].as<std::string>();
        configYaml["log"]["logLevel"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'log' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["server"]["listenIp"].as<std::string>();
        configYaml["server"]["listenPort"].as<unsigned short>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'server' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["agent"]["listenIp"].as<std::string>();
        configYaml["agent"]["listenPort"].as<unsigned short>();
        configYaml["agent"]["serverIp"].as<std::string>();
        configYaml["agent"]["serverPort"].as<unsigned short>();
        configYaml["agent"]["httpVersion"].as<std::string>();
        configYaml["agent"]["userAgent"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'agent' block: ") + e.what() + "\n";
        return result;
    }

    const char *side = mode == "agent" ? "agent" : "server";
    std::string key;
    try {
        auto check = [&](const char *name, auto sample) {
            key = name;
            const YAML::Node node = movedSetting(configYaml, side, name);
            if (node) node.as<decltype(sample)>();
        };
        if (mode == "agent") {
            check("protocol", std::string());
            check("fakeUrls", std::vector<std::string>());
            check("methods", std::vector<std::string>());
            check("endPoints", std::vector<std::string>());
            check("pullTimeout", static_cast<unsigned short>(0));
            check("connectionReuse", false);
            check("tlsVerifyPeer", false);
            check("tlsCaFile", std::string());
        } else {
            check("bufferSize", 0u);
            check("tlsCertFile", std::string());
            check("tlsKeyFile", std::string());
        }
    } catch (const std::exception &e) {
        result.message = std::string("Error in ") + side + "." + key + ": " + e.what() + "\n";
        return result;
    }

    if (mode == "agent") {
        const std::string protocol = movedSetting(configYaml, "agent", "protocol").as<std::string>("");
        if (protocol != "http" && protocol != "socks5") {
            result.message = "agent.protocol must be one of [http|socks5]\n";
            return result;
        }
    } else {
        const unsigned int bufferSize = movedSetting(configYaml, "server", "bufferSize").as<unsigned int>(65536);
        if (bufferSize < 1 || bufferSize > 65536) {
            result.message = "server.bufferSize must be between 1 and 65536\n";
            return result;
        }
    }

    const std::string logLevel = configYaml["log"]["logLevel"].as<std::string>();
    if (logLevel != "INFO" && logLevel != "TRACE" && logLevel != "DEBUG") {
        result.message = "logLevel must be one of [INFO|TRACE|DEBUG]\n";
        return result;
    }

    const bool aesEnable = configYaml["general"]["aesEnable"].as<bool>(true);

    std::string agentToken;
    try {
        agentToken = configYaml["agent"]["token"].as<std::string>("");
    } catch (const std::exception &e) {
        result.message = std::string("Error in agent.token: ") + e.what() + "\n";
        return result;
    }
    const std::string legacyToken = configYaml["general"]["token"].as<std::string>("");

    for (const char *block: {"users", "usage"}) {
        if (configYaml[block]) {
            result.message = std::string(block) + " belongs under the server block: move it to server." +
                             block + "\n";
            return result;
        }
    }

    const YAML::Node users = configYaml["server"]["users"];
    const bool serverUsers = mode == "server" && users && !users.IsNull();

    if (mode == "agent") {
        const std::string token = agentToken.empty() ? legacyToken : agentToken;
        if (token.empty()) {
            result.message = "agent.token is required\n";
            return result;
        }
        if (token.size() < MinTokenLength) {
            result.message = "agent.token must be at least 32 bytes\n";
            return result;
        }
    } else if (serverUsers) {
        if (!users.IsSequence()) {
            result.message = "server.users must be a list\n";
            return result;
        }
        if (users.size() == 0) {
            result.message = "server.users must have at least one entry\n";
            return result;
        }

        std::vector<std::string> names;
        std::vector<std::string> tokens;
        for (std::size_t i = 0; i < users.size(); ++i) {
            const YAML::Node user = users[i];
            const std::string where = "server.users[" + std::to_string(i) + "]";
            std::string name;
            std::string userToken;
            try {
                if (!user.IsMap()) throw std::runtime_error("must be a map with name and token");
                name = user["name"].as<std::string>();
                userToken = user["token"].as<std::string>();
                user["enabled"].as<bool>(true);
                if (user["limit"]) {
                    std::uint64_t limit = 0;
                    if (!parseByteSize(user["limit"].as<std::string>(), limit)) {
                        throw std::runtime_error(
                                "limit must be a size with a unit, such as 2GB, 500MB or 1.5GiB");
                    }
                }
            } catch (const std::exception &e) {
                result.message = "Error in " + where + ": " + e.what() + "\n";
                return result;
            }
            if (!isValidUserName(name)) {
                result.message = where + ".name must be 1-64 characters of [A-Za-z0-9._@-]\n";
                return result;
            }
            if (userToken.size() < MinTokenLength) {
                result.message = "token of user " + name + " must be at least " +
                                 std::to_string(MinTokenLength) + " bytes\n";
                return result;
            }
            for (const auto &n: names) {
                if (n == name) {
                    result.message = "user name " + name + " is used more than once\n";
                    return result;
                }
            }
            for (std::size_t j = 0; j < tokens.size(); ++j) {
                if (tokens[j] == userToken) {
                    result.message = "users " + names[j] + " and " + name + " have the same token\n";
                    return result;
                }
            }
            names.push_back(name);
            tokens.push_back(userToken);
        }
        if (const YAML::Node usage = configYaml["server"]["usage"]) {
            try {
                usage["file"].as<std::string>("");
                const int resetDay = usage["resetDay"].as<int>(1);
                if (resetDay < 1 || resetDay > 28) {
                    throw std::runtime_error("resetDay must be between 1 and 28");
                }
            } catch (const std::exception &e) {
                result.message = std::string("Error in server.usage: ") + e.what() + "\n";
                return result;
            }
        }
    } else if (aesEnable) {
        if (legacyToken.empty()) {
            result.message = "the server needs a server.users list with at least one user\n";
            return result;
        }
        if (legacyToken.size() < MinTokenLength) {
            result.message = "general.token must be at least 32 bytes when aesEnable is true\n";
            return result;
        }
    }

    result.ok = true;
    result.message = "OK";
    return result;
}

inline BoolStr validateConfig(int argc, const char *argv[]) {
    if (argc != 3) {
        return {false,
                "Requires 2 arguments: mode [server|agent] and config file "
                "path\nExample: nipovpn server config.yaml\n"};
    }
    return validateConfigFile(argv[1], argv[2]);
}