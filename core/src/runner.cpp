#include "runner.hpp"

/**
 * @brief Constructs a `Runner` instance with configuration and logging support.
 *
 * @details
 * - Stores references to the configuration and logging objects.
 * - Initializes the internal Boost.Asio `io_context`.
 * - Creates a work guard to keep the `io_context` alive.
 * - Sets the running state to `true`.
 * - Initializes a strand for serialized handler execution.
 *
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
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
 * @brief Destroys the `Runner` instance and shuts down all execution resources.
 *
 * @details
 * - Sets the running state to `false`.
 * - Stops the Boost.Asio `io_context`.
 * - Joins all worker threads in the thread pool if they are still joinable.
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
 * @brief Starts the server runtime and launches worker threads.
 *
 * @details
 * - Logs the current runtime mode and full configuration.
 * - Creates the `TCPServer` instance bound to the internal `io_context`.
 * - Spawns a number of worker threads based on the configured thread count.
 * - Each worker thread runs the `workerThread()` loop.
 * - Waits for all worker threads to finish before returning.
 * - Logs any exception that occurs during startup or runtime coordination.
 */
void Runner::run() {
    try {
        log_->write("Config initialized in " + config_->modeToString() + " mode", Log::Level::INFO);
        log_->write(config_->toString(), Log::Level::INFO);

        auto tcpServer = TCPServer::create(io_context_, config_, log_);

        for (auto i = 0; i < config_->threads(); ++i) {
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
 * @brief Worker thread entry point for processing asynchronous operations.
 *
 * @details
 * - Repeatedly runs the Boost.Asio `io_context` while the runner is active.
 * - Exits the loop when `io_context_.run()` completes normally.
 * - Catches and logs exceptions so that a single handler failure does not
 *   immediately terminate the worker loop.
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
 * @brief Stops the runner and terminates asynchronous processing.
 *
 * @details
 * - Sets the running state to `false`.
 * - Stops the internal Boost.Asio `io_context`.
 */
void Runner::stop() {
    running_.store(false);
    io_context_.stop();
}