#include "runner.hpp"

Runner::Runner(const std::shared_ptr<Config>& config, 
  const std::shared_ptr<Log>& log)
    :
      config_(config),
      log_(log)
  { }

Runner::~Runner()
{}

void Runner::run()
{
  try
  {
    boost::asio::io_context io_context_{config_->threads()};
    log_->write("Config initialized in " + config_->modeToString() + " mode ", Log::Level::INFO);
    log_->write(config_->toString(), Log::Level::DEBUG);
    TCPServer::pointer tcpServer_ = TCPServer::create(io_context_, config_, log_);
    std::vector<std::thread> vectorThreads_;
    vectorThreads_.reserve(config_->threads() - 1);
    for(auto i = config_->threads() - 1; i > 0; --i)
        vectorThreads_.emplace_back(
        [&io_context_]
        {
            io_context_.run();
        });
    io_context_.run();
  }
  catch (std::exception& error)
  {
    log_->write(std::string("[Runner run] ") + error.what(), Log::Level::ERROR);
  }
}