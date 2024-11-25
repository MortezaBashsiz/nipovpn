#include <iostream>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "runner.hpp"

int main(int argc, char const *argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <mode> <config_file_path>\n";
        return 1;
    }

    BoolStr configValidation = validateConfig(argc, argv);
    if (!configValidation.ok) {
        std::cerr << "Configuration validation failed: " << configValidation.message
                  << "\n";
        return 1;
    }

    std::string mode(argv[1]);
    RunMode runMode_ = (mode == "agent") ? RunMode::agent : RunMode::server;

    Config::pointer config_ = Config::create(runMode_, std::string(argv[2]));

    Log::pointer log_ = Log::create(config_);

    Runner runner_(config_, log_);
    runner_.run();

    return 0;
}