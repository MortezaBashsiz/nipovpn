#include "runner.hpp"

Runner::Runner(const std::shared_ptr<Config>& config,
               const std::shared_ptr<Log>& log)
    : config_(config),
      log_(log),
      io_context_(),
      workGuard_(boost::asio::make_work_guard(io_context_)) {}

Runner::~Runner() {}

void Runner::run() {
  try {
    log_->write("Config initialized in " + config_->modeToString() + " mode",
                Log::Level::INFO);
    log_->write(config_->toString(), Log::Level::INFO);
    TCPServer::pointer tcpServer = TCPServer::create(io_context_, config_, log_);
    std::vector<std::thread> threadPool;
    threadPool.reserve(config_->threads() - 1);
    for (auto i = 0; i < config_->threads() - 1; ++i) {
      threadPool.emplace_back([this] { io_context_.run(); });
    }
    io_context_.run();
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