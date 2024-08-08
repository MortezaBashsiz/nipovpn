#pragma once

#ifndef RUNNER_HPP
#define RUNNER_HPP

#include <atomic>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "config.hpp"
#include "log.hpp"
#include "tcpserver.hpp"

/*
 * The Runner class is responsible for managing the application's main execution
 * loop. It initializes the server, starts worker threads, and handles
 * exceptions. This class is used in the main function to run the I/O context
 * and manage the server lifecycle.
 */
class Runner : private Uncopyable {
 public:
  /*
   * Constructor to initialize the Runner object with configuration and logging
   * objects.
   * @param config - Shared pointer to the configuration object.
   * @param log - Shared pointer to the logging object.
   */
  explicit Runner(const std::shared_ptr<Config>& config,
                  const std::shared_ptr<Log>& log);

  /*
   * Destructor to clean up resources, stop the I/O context, and join all
   * threads.
   */
  ~Runner();

  /*
   * Main function to start the server and run the I/O context.
   * This function sets up the server, starts worker threads, and manages
   * execution.
   */
  void run();

 private:
  /*
   * Worker thread function that processes asynchronous operations.
   * This function runs the I/O context and handles exceptions.
   */
  void workerThread();

  void stop();

  // Configuration object for the server.
  const std::shared_ptr<Config>& config_;

  // Logging object for recording messages and errors.
  const std::shared_ptr<Log>& log_;

  // Boost.Asio I/O context to manage asynchronous operations.
  boost::asio::io_context io_context_;

  // Work guard to prevent the I/O context from running out of work.
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_guard_;

  // Atomic flag to indicate if the server is still running.
  std::atomic<bool> running_;

  // Thread pool to manage worker threads.
  std::vector<std::thread> threadPool_;
};

#endif /* RUNNER_HPP */
