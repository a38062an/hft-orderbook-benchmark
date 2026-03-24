#pragma once

#include "core/Order.hpp"
#include "utils/lock_free_queue.hpp"
#include <atomic>
#include <thread>
#include <vector>

namespace hft
{

class MetricsCollector;

class TCPOrderGateway
{
  public:
    TCPOrderGateway(int port, LockFreeQueue<Order, 1024> &queue);
    ~TCPOrderGateway();

    void start();
    void stop();

    void setMetricsCollector(const MetricsCollector *metrics)
    {
        metrics_ = metrics;
    }

  private:
    void acceptLoop();
    void clientHandler(int clientSock);

    int serverSocket_;
    int port_;
    LockFreeQueue<Order, 1024> &orderQueue_;
    std::atomic<bool> running_;
    const MetricsCollector *metrics_ = nullptr;
    std::jthread acceptConnectionThread_;
    std::vector<std::jthread> clientThreads_;
};

} // namespace hft
