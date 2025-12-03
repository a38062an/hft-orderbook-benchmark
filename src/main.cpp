#include "core/MatchingEngine.hpp"
#include "network/TCPOrderGateway.hpp"
#include "utils/LockFreeQueue.hpp"
#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>

using namespace hft;

std::atomic<bool> isApplicationRunning{true};

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received. Stopping..." << std::endl;
    isApplicationRunning.store(false, std::memory_order_relaxed);
}

int main() 
{
    // Register signal handler for graceful shutdown
    signal(SIGINT, signalHandler);

    std::cout << "Initializing HFT Orderbook Benchmark Server..." << std::endl;

    // 1. Initialize Shared Queue (SPSC)
    // Capacity must be power of 2
    LockFreeQueue<Order, 1024> orderQueue;

    // 2. Initialize Components
    // Gateway produces to queue
    TCPOrderGateway gateway(12345, orderQueue);
    
    // Engine consumes from queue
    MatchingEngine engine(orderQueue);

    // 3. Start Network Thread
    std::cout << "Starting TCP Gateway on port 12345..." << std::endl;
    gateway.start();

    // 4. Run Matching Engine (Worker Thread)
    // In a real HFT setup, we would pin this thread to a specific core here.
    std::cout << "Starting Matching Engine Loop..." << std::endl;
    engine.run(isApplicationRunning);

    // 5. Shutdown
    std::cout << "Stopping Gateway..." << std::endl;
    gateway.stop();

    std::cout << "=== Final Statistics ===" << std::endl;
    std::cout << "Total Orders: " << engine.getMetrics().getOrderCount() << std::endl;
    std::cout << "Total Trades: " << engine.getMetrics().getTradeCount() << std::endl;

    return 0;
}
