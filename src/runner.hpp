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
 * This class is responsible to run the program and handle the exceptions
 * It is called in the main function (see main.cpp)
 */
class Runner : private Uncopyable {
 public:
  explicit Runner(const std::shared_ptr<Config>& config,
                  const std::shared_ptr<Log>& log);
  ~Runner();

  /*
   * It is called in the main function (see main.cpp) and will run the
   * io_context
   */
  void run();

 private:
  void workerThread();

  const std::shared_ptr<Config>& config_;
  const std::shared_ptr<Log>& log_;
  boost::asio::io_context io_context_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_guard_;
  std::atomic<bool> running_;
  std::vector<std::thread> threadPool_;
};

#endif /* RUNNER_HPP */
