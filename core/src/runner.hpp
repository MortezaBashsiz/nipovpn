#pragma once

#include <atomic>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <vector>

#if defined(__ANDROID__)
using runnerThread = std::thread;
#else
using runnerThread = std::jthread;
#endif

#include "config.hpp"
#include "log.hpp"
#include "tcpserver.hpp"

class Runner : private Uncopyable {
public:
    explicit Runner(const std::shared_ptr<Config> &config,
                    const std::shared_ptr<Log> &log);

    ~Runner();

    void run();

private:
    void workerThread();

    void stop();

    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;

    boost::asio::io_context io_context_;

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;

    std::atomic<bool> running_;

    std::vector<runnerThread> threadPool_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};