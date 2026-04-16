#include <iostream>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "runner.hpp"

/**
 * @brief Application entry point.
 *
 * @details
 * - Validates command-line arguments.
 * - Validates the provided configuration file and runtime mode.
 * - Creates the main configuration and logging objects.
 * - Constructs the application runner and starts execution.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return `0` on successful execution, otherwise `1`.
 */
int main(int argc, char const *argv[]) {

    /**
     * @brief Ensures that the required command-line arguments are provided.
     *
     * @details
     * - Expects two arguments:
     *   - runtime mode (`server` or `agent`)
     *   - configuration file path
     */
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <mode> <config_file_path>\n";
        return 1;
    }

    /**
     * @brief Validates the configuration file and command-line arguments.
     */
    BoolStr configValidation = validateConfig(argc, argv);
    if (!configValidation.ok) {
        std::cerr << "Configuration validation failed: " << configValidation.message
                  << "\n";
        return 1;
    }

    /**
     * @brief Converts the string mode argument into the internal `RunMode` enum.
     */
    std::string mode(argv[1]);
    RunMode runMode_ = (mode == "agent") ? RunMode::agent : RunMode::server;

    /**
     * @brief Creates the configuration object from the provided file.
     */
    Config::pointer config_ = Config::create(runMode_, std::string(argv[2]));

    /**
     * @brief Creates the logging object using the loaded configuration.
     */
    Log::pointer log_ = Log::create(config_);

    /**
     * @brief Creates the main runner instance and starts the application.
     */
    Runner runner_(config_, log_);
    runner_.run();

    return 0;
}