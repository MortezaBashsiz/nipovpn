#include <iostream>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "runner.hpp"

int
main (int argc, char const *argv[])
{
  // Ensure that at least two arguments are provided: the mode and the config
  // file path.
  if (argc < 3)
    {
      std::cerr << "Usage: " << argv[0] << " <mode> <config_file_path>\n";
      return 1; // Exit with error code 1 indicating incorrect usage.
    }

  // Validate the configuration using the provided arguments.
  BoolStr configValidation = validateConfig (argc, argv);
  if (!configValidation.ok)
    {
      std::cerr << "Configuration validation failed: "
                << configValidation.message << "\n";
      return 1; // Exit with error code 1 indicating configuration validation
                // failure.
    }

  // Determine the run mode based on the first argument.
  // If "agent" is specified, set runMode_ to RunMode::agent; otherwise, set it
  // to RunMode::server.
  std::string mode (argv[1]);
  RunMode runMode_ = (mode == "agent") ? RunMode::agent : RunMode::server;

  // Initialize the main Config object with the run mode and config file path.
  Config::pointer config_ = Config::create (runMode_, std::string (argv[2]));

  // Initialize the main Log object using the created Config object.
  Log::pointer log_ = Log::create (config_);

  // Create and run the Runner object with the Config and Log objects.
  Runner runner_ (config_, log_);
  runner_.run ();

  return 0; // Exit with success code 0.
}
