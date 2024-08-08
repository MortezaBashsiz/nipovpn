#include "runner.hpp"

// Constructor for the Runner class
Runner::Runner(const std::shared_ptr<Config>& config,
               const std::shared_ptr<Log>& log)
    : config_(config),  // Store the configuration object
      log_(log),        // Store the log object
      io_context_(),  // Initialize the I/O context for asynchronous operations
      work_guard_(boost::asio::make_work_guard(
          io_context_)),  // Prevents io_context from running out of work
      running_(true) {}   // Initialize running state to true

// Destructor for the Runner class
Runner::~Runner() {
  running_.store(
      false);  // Set the running flag to false to stop the worker threads
  io_context_.stop();  // Stop the I/O context, which will end the processing
  for (auto& thread : threadPool_) {  // Wait for all threads to complete
    if (thread.joinable()) {
      thread.join();
    }
  }
}

// Main function to run the server
void Runner::run() {
  try {
    // Log the configuration mode and details
    log_->write("Config initialized in " + config_->modeToString() + " mode",
                Log::Level::INFO);
    log_->write(config_->toString(), Log::Level::INFO);

    // Create and initialize the TCP server
    auto tcpServer = TCPServer::create(io_context_, config_, log_);

    // Start initial worker threads
    for (auto i = 0; i < config_->threads(); ++i) {
      threadPool_.emplace_back([this] { workerThread(); });
    }

    // Wait for all threads to finish
    for (auto& thread : threadPool_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  } catch (const std::exception& error) {
    // Log any exceptions thrown during execution
    log_->write(std::string("[Runner run] ") + error.what(), Log::Level::ERROR);
  } catch (...) {
    // Log any unknown exceptions
    log_->write("[Runner run] Unknown error occurred", Log::Level::ERROR);
  }
}

// Function for worker threads to process tasks
void Runner::workerThread() {
  while (running_.load()) {
    try {
      io_context_.run();
      break;  // Exit loop if io_context_.run() completes
    } catch (const std::exception& e) {
      log_->write(std::string("[WorkerThread] Exception: ") + e.what(),
                  Log::Level::ERROR);
    } catch (...) {
      log_->write("[WorkerThread] Unknown error occurred", Log::Level::ERROR);
    }
  }
}

// Function to stop thread
void Runner::stop() {
  running_.store(false);
  io_context_.stop();
}
