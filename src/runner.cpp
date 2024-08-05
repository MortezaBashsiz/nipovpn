#include "runner.hpp"

Runner::Runner(const std::shared_ptr<Config>& config,
               const std::shared_ptr<Log>& log)
    : config_(config), log_(log) {}

Runner::~Runner() {}

void Runner::run() {
  try {
    boost::asio::io_context io_context(config_->threads());
    log_->write("Config initialized in " + config_->modeToString() + " mode",
                Log::Level::INFO);
    log_->write(config_->toString(), Log::Level::INFO);
    TCPServer::pointer tcpServer = TCPServer::create(io_context, config_, log_);
    std::vector<std::thread> threadPool;
    threadPool.reserve(config_->threads() - 1);
    for (auto i = 0; i < config_->threads() - 1; ++i) {
      threadPool.emplace_back([&io_context] { io_context.run(); });
    }
    io_context.run();
    for (auto& thread : threadPool) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  } catch (const std::exception& error) {
    log_->write(std::string("[Runner run] ") + error.what(), Log::Level::ERROR);
  } catch (...) {
    log_->write("[Runner run] Unknown error occurred", Log::Level::ERROR);
  }
}