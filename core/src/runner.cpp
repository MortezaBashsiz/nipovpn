#include "runner.hpp"

/**
 * @brief Constructs a `Runner` object with the given configuration and log instance.
 *
 * Initializes the `io_context_`, work guard, and strand. Sets the `running_` flag to true.
 *
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging instance.
 */
Runner::Runner(const std::shared_ptr<Config> &config,
               const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(),
      work_guard_(boost::asio::make_work_guard(io_context_)),
      running_(true),
      strand_(boost::asio::make_strand(io_context_)) {}

/**
 * @brief Destructor for the `Runner` class.
 *
 * Stops the `io_context_`, ensures all threads in the thread pool are joined, and safely cleans up resources.
 */
Runner::~Runner() {
    running_.store(false);
    io_context_.stop();
    for (auto &thread: threadPool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

/**
 * @brief Starts the Runner's main execution.
 *
 * Initializes the server and spawns multiple worker threads to handle tasks asynchronously.
 * Joins all threads to ensure proper shutdown.
 */
void Runner::run() {
    try {

        log_->write("Config initialized in " + config_->modeToString() + " mode", Log::Level::INFO);
        log_->write(config_->toString(), Log::Level::INFO);
        auto tcpServer = TCPServer::create(io_context_, config_, log_);
        for (auto i = 0; i < config_->threads() * 50; ++i) {
            threadPool_.emplace_back([this] { workerThread(); });
        }
        for (auto &thread: threadPool_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    } catch (const std::exception &error) {
        log_->write(std::string("[Runner run] ") + error.what(), Log::Level::ERROR);
    } catch (...) {
        log_->write("[Runner run] Unknown error occurred", Log::Level::ERROR);
    }
}

/**
 * @brief Worker thread function to process tasks from the `io_context_`.
 *
 * Each thread calls `io_context_.run()` to handle asynchronous operations. If an exception occurs,
 * it logs the error and continues processing.
 */
void Runner::workerThread() {
    while (running_.load()) {
        try {
            io_context_.run();
            break;
        } catch (const std::exception &e) {

            log_->write(std::string("[WorkerThread] Exception: ") + e.what(), Log::Level::ERROR);
        } catch (...) {

            log_->write("[WorkerThread] Unknown error occurred", Log::Level::ERROR);
        }
    }
}

/**
 * @brief Stops the Runner.
 *
 * Sets the `running_` flag to false and stops the `io_context_` to terminate the processing loop.
 */
void Runner::stop() {
    running_.store(false);
    io_context_.stop();
}
