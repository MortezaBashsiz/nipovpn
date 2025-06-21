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
 * @class Runner
 * @brief Manages the execution of an asynchronous Boost.Asio-based server.
 *
 * The `Runner` class sets up a multi-threaded environment for handling
 * asynchronous tasks using Boost.Asio. It provides mechanisms to run, manage,
 * and stop the server efficiently.
 */
class Runner : private Uncopyable {
public:
    /**
     * @brief Constructs the `Runner` object with the specified configuration and logging instance.
     *
     * Initializes the Boost.Asio context and sets up the necessary guards and threading environment.
     *
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging instance.
     */
    explicit Runner(const std::shared_ptr<Config> &config,
                    const std::shared_ptr<Log> &log);

    /**
     * @brief Destructor for the `Runner` class.
     *
     * Ensures that the server and its threads are gracefully stopped.
     */
    ~Runner();

    /**
     * @brief Starts the asynchronous server and its worker threads.
     *
     * This method runs the Boost.Asio `io_context` to process asynchronous tasks.
     */
    void run();

private:
    /**
     * @brief Executes tasks in a worker thread.
     *
     * Each worker thread runs the `io_context`, allowing it to process asynchronous operations.
     */
    void workerThread();

    /**
     * @brief Stops the server and shuts down all threads gracefully.
     *
     * Signals the termination of the server and cleans up resources.
     */
    void stop();

    const std::shared_ptr<Config> &config_;

    const std::shared_ptr<Log> &log_;

    boost::asio::io_context io_context_;

    /**
     * @brief Work guard to keep the `io_context` running.
     *
     * Ensures that the `io_context` does not exit when there are no tasks to process.
     */
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;

    std::atomic<bool> running_;

    std::vector<std::jthread> threadPool_;

    /**
     * @brief Strand to ensure serialized execution of tasks within the `io_context`.
     *
     * Provides thread safety for handlers that must not run concurrently.
     */
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};
