#pragma once

#include <atomic>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "config.hpp"
#include "log.hpp"
#include "tcpserver.hpp"

/**
 * @brief The `Runner` class manages the lifecycle and execution of the asynchronous server.
 *
 * @details
 * - Initializes and owns the Boost.Asio `io_context`.
 * - Creates and manages a pool of worker threads.
 * - Keeps the `io_context` alive using a work guard.
 * - Provides mechanisms to start and gracefully stop the server.
 * - Ensures thread-safe execution of handlers using a strand.
 *
 * @note
 * - Inherits from `Uncopyable` to prevent copying.
 * - Uses `std::jthread` for automatic thread cleanup on destruction.
 */
class Runner : private Uncopyable {
public:
    /**
     * @brief Constructs a `Runner` instance with configuration and logging support.
     *
     * @details
     * - Initializes the internal `io_context`.
     * - Creates a work guard to prevent premature exit of the event loop.
     * - Prepares the threading and execution environment.
     *
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit Runner(const std::shared_ptr<Config> &config,
                    const std::shared_ptr<Log> &log);

    /**
     * @brief Destructor for `Runner`.
     *
     * @details
     * - Ensures that all worker threads are stopped.
     * - Releases the work guard and shuts down the `io_context`.
     */
    ~Runner();

    /**
     * @brief Starts the server and worker thread pool.
     *
     * @details
     * - Launches multiple worker threads.
     * - Each thread runs the `io_context` event loop.
     * - Begins processing asynchronous operations.
     */
    void run();

private:
    /**
     * @brief Worker thread entry point.
     *
     * @details
     * - Executes the `io_context::run()` loop.
     * - Processes asynchronous tasks until the context is stopped.
     */
    void workerThread();

    /**
     * @brief Stops the server and shuts down execution.
     *
     * @details
     * - Signals termination via the `running_` flag.
     * - Stops the `io_context`.
     * - Ensures all threads exit cleanly.
     */
    void stop();

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;      ///< Reference to the logging object.

    boost::asio::io_context io_context_;///< Core I/O execution context.

    /**
     * @brief Work guard to keep the `io_context` active.
     *
     * @details
     * - Prevents `io_context::run()` from returning when no tasks are queued.
     */
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;

    std::atomic<bool> running_;///< Indicates whether the runner is active.

    std::vector<std::jthread> threadPool_;///< Pool of worker threads.

    /**
     * @brief Strand for serialized handler execution.
     *
     * @details
     * - Ensures that certain handlers do not execute concurrently.
     */
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};