#include "log.hpp"

Log::Log(const std::shared_ptr<Config>& config)
  :
    config_(config)
{
  std::ofstream logFile_(config_->log().file, logFile_.out | logFile_.app);
  /*
  * Checks if the log file is there or not
  */
  if (! logFile_.is_open())
    std::cerr << "Error openning log file : " << config_->log().file  << " make sure the directory and file exists " << "\n";
  /*
  * Checks and validates the level defined in the config.yaml
  * Also initialize the Log::Level
  */
  if (config_->log().level == std::string("INFO"))
    level_ = Level::INFO;
  else if (config_->log().level == std::string("DEBUG"))
    level_ = Level::DEBUG;
  else
    std::cerr << "Invalid log level : " << config_->log().level  << ", It should be one of [INFO|DEBUG] " << "\n";
  logFile_.close();
}

Log::Log(const std::shared_ptr<Log>& log)
  :
    config_(log->config_),
    logFileClosed(log->logFileClosed),
    level_(log->level_)
{}

Log::~Log()
{}

void Log::write(const std::string& message, const Level& level) const
{
  std::ofstream logFile_(config_->log().file, logFile_.out | logFile_.app);
  if (level <= level_ || level == Level::ERROR){
    if (logFile_.is_open())
    {
      auto time = std::time(nullptr);
      auto localtime = *std::localtime(&time);
      std::ostringstream oss;
      oss << std::put_time(&localtime, "%Y-%m-%d_%H:%M:%S");
      auto timeStr = oss.str();
      std::string line = timeStr + " [" + levelToString(level) + "] " + message + "\n";
      logFile_ << line;
      std::cout << line;
      logFile_.close();
    } else
    {
      std::cerr << "Error openning log file : " << config_->log().file  << " make sure the directory and file exists " << "\n";
    }
  }
}