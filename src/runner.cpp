#include "runner.hpp"

Runner::Runner(const std::shared_ptr<Config>& config,
               const std::shared_ptr<Log>& log)
    : config_(config),
      log_(log),
      io_context_(),
      work_guard_(boost::asio::make_work_guard(io_context_)),
      running_(true) {}

Runner::~Runner() {
  running_.store(false);
  io_context_.stop();
  for (auto& thread : threadPool_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void Runner::run() {
  try {
    log_->write("Config initialized in " + config_->modeToString() + " mode",
                Log::Level::INFO);
    log_->write(config_->toString(), Log::Level::INFO);
    TCPServer::pointer tcpServer =
        TCPServer::create(io_context_, config_, log_);

    // Start initial threads
    for (auto i = 0; i < config_->threads(); ++i) {
      threadPool_.emplace_back([this] { workerThread(); });
    }

    // Main loop to create new threads
    while (running_.load()) {
      threadPool_.emplace_back([this] { workerThread(); });
      std::this_thread::sleep_for(
          std::chrono::seconds(1));  // Adjust sleep duration as needed
    }

    // Ensure all threads are joined
    for (auto& thread : threadPool_) {
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

void Runner::workerThread() {
  while (running_.load()) {
    try {
      io_context_.run();
    } catch (const std::exception& e) {
      log_->write(std::string("[WorkerThread] Exception: ") + e.what(),
                  Log::Level::ERROR);
    } catch (...) {
      log_->write("[WorkerThread] Unknown exception", Log::Level::ERROR);
    }
  }
}
