#pragma once

#include "core/Order.hpp"
#include "utils/LockFreeQueue.hpp"
#include <atomic>
#include <thread>
#include <vector>

namespace hft 
{

class TCPOrderGateway 
{
public:
    TCPOrderGateway(int port, LockFreeQueue<Order, 1024>& queue);
    ~TCPOrderGateway();

    void start();
    void stop();

private:
    void acceptLoop();
    void clientHandler(int clientSock);

    int serverSocket_;
    int port_;
    LockFreeQueue<Order, 1024>& orderQueue_;
    std::atomic<bool> running_;
    std::jthread acceptConnectionThread_;
    std::vector<std::jthread> clientThreads_;
};

} // namespace hft
