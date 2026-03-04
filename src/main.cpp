#include "core/matching_engine.hpp"
#include "network/tcp_order_gateway.hpp"
#include "orderbooks/map_order_book.hpp"
#include "utils/lock_free_queue.hpp"
#include "utils/thread_pinning.hpp"
#include <iostream>
#include <atomic>
#include <csignal>


using namespace hft;

std::atomic<bool> isApplicationRunning{true};

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received. Stopping..." << std::endl;
    isApplicationRunning.store(false, std::memory_order_relaxed);
}

/**
 * Currently tests map order book
 */
int main()
{
    // Register signal handler for graceful shutdown
    signal(SIGINT, signalHandler);

    std::cout << "Initializing HFT Orderbook Benchmark Server..." << std::endl;

    LockFreeQueue<Order, 1024> orderQueue; 
    MapOrderBook orderBook; // Can be swapped out for other order book implementations
    TCPOrderGateway gateway(12345, orderQueue);
    MatchingEngine engine(orderQueue, orderBook);

    // Start Gateway to start accepting clients
    std::cout << "Starting TCP Gateway on port 12345..." << std::endl;
    gateway.start();

    // Start Matching Engine Loop to consume from queue and process via order book
    if (hft::pinToCore(0)) {
        std::cout << "Matching Engine thread successfully pinned to core 0." << std::endl;
    } else {
        std::cerr << "Warning: Failed to pin thread to core 0." << std::endl;
    }
    std::cout << "Starting Matching Engine Loop..." << std::endl;
    engine.run(isApplicationRunning);

    // Stop Gateway when application is shutting down
    std::cout << "Stopping Gateway..." << std::endl;
    gateway.stop();

    std::cout << "=== Final Statistics ===" << std::endl;
    std::cout << "Total Orders: " << engine.getMetrics().getOrderCount() << std::endl;
    std::cout << "Total Trades: " << engine.getMetrics().getTradeCount() << std::endl;

    return 0;
}
