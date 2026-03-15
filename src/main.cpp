#include "core/matching_engine.hpp"
#include "core/order_book_factory.hpp"
#include "network/tcp_order_gateway.hpp"
#include "utils/lock_free_queue.hpp"
#include "utils/thread_pinning.hpp"
#include <atomic>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>

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
int main(int argc, char *argv[])
{
    // Register signal handler for graceful shutdown
    signal(SIGINT, signalHandler);

    int port = 12345;
    std::string bookType = "map";
    std::string csvOut = "";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc)
        {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--book" && i + 1 < argc)
        {
            bookType = argv[++i];
        }
        else if (arg == "--csv_out" && i + 1 < argc)
        {
            csvOut = argv[++i];
        }
    }

    std::cout << "Initializing HFT Exchange Server..." << std::endl;
    std::cout << "Selected OrderBook: " << bookType << std::endl;

    try
    {
        auto orderBook = OrderBookFactory::create(bookType);
        LockFreeQueue<Order, 1024> orderQueue;
        TCPOrderGateway gateway(port, orderQueue);
        MatchingEngine engine(orderQueue, *orderBook);
        
        // Link metrics to gateway so it can report stats to clients
        gateway.setMetricsCollector(&engine.getMetrics());

        // Start Gateway to start accepting clients
        std::cout << "Starting TCP Gateway on port " << port << "..." << std::endl;
        gateway.start();

        // Start Matching Engine Loop
        if (hft::pinToCore(0))
        {
            std::cout << "Matching Engine thread pinned to core 0." << std::endl;
        }
        else
        {
            std::cerr << "Warning: Failed to pin thread to core 0." << std::endl;
        }
        std::cout << "Starting Matching Engine Loop..." << std::endl;
        engine.run(isApplicationRunning);

        // Cleanup
        gateway.stop();

        std::cout << "=== Final Statistics ===" << std::endl;
        auto stats = engine.getMetrics().getStats();
        std::cout << "Total Orders processed: " << engine.getMetrics().getOrderCount() << std::endl;
        std::cout << "Total Trades executed:  " << engine.getMetrics().getTradeCount() << std::endl;
        std::cout << "--- Wire-to-Match Latency ---" << std::endl;
        std::cout << "  Mean Latency: " << std::fixed << std::setprecision(2) << stats.mean << " ticks" << std::endl;
        std::cout << "  P99 Latency:  " << stats.p99 << " ticks" << std::endl;
        std::cout << "  Max Latency:  " << stats.max << " ticks" << std::endl;

        // Write stats to CSV if csvOut is specified
        if (!csvOut.empty())
        {
            std::ifstream checkFile(csvOut);
            bool exists = checkFile.good();
            checkFile.close();

            std::ofstream outFile(csvOut, std::ios::app);
            if (!exists)
            {
                outFile << "Mode,Book,Mean_ticks,P99_ticks,Max_ticks,Processed\n";
            }
            outFile << "server_wire_to_match," << bookType << "," << stats.mean << "," << stats.p99 << "," << stats.max << "," << engine.getMetrics().getOrderCount() << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
