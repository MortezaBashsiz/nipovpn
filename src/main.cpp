#include <iostream>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "runner.hpp"

int main(int argc, char const *argv[]) {
  // Ensure at least two arguments are passed (the mode and the config file
  // path)
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <mode> <config_file_path>\n";
    return 1;
  }
  // Validate configuration
  BoolStr configValidation = validateConfig(argc, argv);
  if (!configValidation.ok) {
    std::cerr << "Configuration validation failed: " << configValidation.message
              << "\n";
    return 1;
  }
  // Determine run mode
  std::string mode(argv[1]);
  RunMode runMode_ = (mode == "agent") ? RunMode::agent : RunMode::server;
  // Initialize main Config object
  Config::pointer config_ = Config::create(runMode_, std::string(argv[2]));
  // Initialize main Log object
  Log::pointer log_ = Log::create(config_);
  // Initialize and run the runner
  Runner runner_(config_, log_);
  runner_.run();
  return 0;
}