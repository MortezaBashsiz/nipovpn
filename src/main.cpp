#include "general.hpp"
#include "config.hpp"
#include "log.hpp"
#include "runner.hpp"

int main(int argc, char const *argv[])
{
  /*
  * Declare and initialize BoolStr item to validate the config
  */
  BoolStr configValidation{false, std::string("FAILED")};

  /*
  * Call validate configuration with argc and argv
  * Check if the configuration is valid or not
  */
  configValidation = validateConfig(argc, argv);
  if (! configValidation.ok)
  {
    std::cerr << configValidation.message;
    return 1;
  }

  /*
  * Check and set the runMode_
  */
  RunMode runMode_ = RunMode::server;
  if (argv[1] == std::string("agent"))
    runMode_ = RunMode::agent;

  /*
  * Declare and initialize main Config object
  * See config.hpp
  */
  Config::pointer config_ = Config::create(runMode_, std::string(argv[2]));

  /*
  * Declare and initialize main Log object
  */
  Log::pointer log_ = Log::create(config_);

  Runner runner_(config_, log_);
  // std::string key = "01234567890123456789012345678901"; // 32-byte key for AES-256
  // std::string plaintext = "This is a test.";

  // std::string encrypted = aes256Encrypt(plaintext, key);
  // std::cout << "Encrypted text: " << encrypted << std::endl;

  // std::string decrypted = aes256Decrypt(encrypted, key);
  // std::cout << "Decrypted text: " << decrypted << std::endl;

  runner_.run();

  return 0;
}