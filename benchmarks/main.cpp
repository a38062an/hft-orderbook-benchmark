#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <string>
#include <chrono>
#include <algorithm>
#include <iomanip>

#include "modules/order_generator.hpp"
#include "modules/order_book_benchmark.hpp"

#include "../src/core/order.hpp"
#include "../src/core/i_order_book.hpp"
#include "../src/utils/thread_pinning.hpp"

// Include all orderbook implementations
#include "../src/orderbooks/map_order_book.hpp"
#include "../src/orderbooks/vector_order_book.hpp"
#include "../src/orderbooks/array_order_book.hpp"
#include "../src/orderbooks/hybrid_order_book.hpp"
#include "../src/orderbooks/pool_order_book.hpp"

using namespace hft;

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (hft::pinToCore(0))
        std::cout << "Thread successfully pinned to core 0 (or affinity tag set)\n";
    else
        std::cerr << "Warning: Failed to pin thread to core 0\n";

    std::cout << "High-Frequency Trading OrderBook Benchmark (Updated)\n";
    std::cout << "==========================================\n\n";

    std::vector<size_t> ORDER_COUNTS = {1000, 10000, 50000, 100000};
    const size_t WARMUP_COUNT = 1000;
    const uint32_t SEED = 42;

    std::vector<std::tuple<std::string, std::string, OperationBreakdown>> allResults;
    std::vector<std::string> implementations = {"MapOrderBook", "VectorOrderBook", "ArrayOrderBook", "HybridOrderBook", "PoolOrderBook"}; 

    std::shuffle(implementations.begin(), implementations.end(), std::default_random_engine(SEED));

    for (size_t orderCount : ORDER_COUNTS)
    {
        std::cout << "\n=== Testing with " << orderCount << " orders ===\n\n";
        OrderGenerator generator(SEED);

        std::cout << "Generating scenarios...\n";
        // Base orders
        auto tightSpreadOrders = generator.generateTightSpread(orderCount);
        auto fixedLevelsOrders = generator.generateFixedLevels(orderCount);
        auto uniformOrders     = generator.generateUniformRandom(orderCount);
        auto highCancelOrders  = generator.generateHighCancellation(orderCount); // 50% reads implicitly via generate? No, just 50% write/cancel.
        
        // For Read/Write Heavy, we use the same order set but change 'readsPerOp'
        auto mixedOrders = generator.generateMixed(orderCount); 

        std::cout << "Done.\n\n";

        std::string countSuffix = "_" + std::to_string(orderCount / 1000) + "K";
        
        struct ScenarioDef {
            std::string name;
            std::vector<Order> orders;
            size_t readsPerOp;
        };

        std::vector<ScenarioDef> scenarios = {
            {"TightSpread" + countSuffix, tightSpreadOrders, 1},    // 1 Read per Write (Balanced)
            {"FixedLevels" + countSuffix, fixedLevelsOrders, 1},
            {"UniformRand" + countSuffix, uniformOrders, 1},
            {"HighCancel" + countSuffix,  highCancelOrders, 1},
            {"ReadHeavy" + countSuffix,   mixedOrders, 9},          // 9 Reads per Write (90% Read)
            {"WriteHeavy" + countSuffix,  mixedOrders, 0}           // 0 Reads per Write (Pure Write)
        };

        for (const auto &implName : implementations)
        {
            std::cout << "Running " << implName << " benchmarks...\n";
            std::cout << std::string(60, '-') << "\n";

            for (const auto &scen : scenarios)
            {
                if (scen.orders.empty()) continue;

                std::unique_ptr<IOrderBook> orderbook;
                if (implName == "MapOrderBook") orderbook = std::make_unique<MapOrderBook>();
                else if (implName == "VectorOrderBook") orderbook = std::make_unique<VectorOrderBook>();
                else if (implName == "ArrayOrderBook") orderbook = std::make_unique<ArrayOrderBook>(5000, 15000, 1);
                else if (implName == "HybridOrderBook") orderbook = std::make_unique<HybridOrderBook>(20);
                else if (implName == "PoolOrderBook") orderbook = std::make_unique<PoolOrderBook>(1000000);

                OrderBookBenchmark benchmark(implName, std::move(orderbook));

                std::cout << "  " << std::left << std::setw(20) << scen.name << "... " << std::flush;
                auto stats = benchmark.runScenario(scen.orders, scen.readsPerOp, std::min(WARMUP_COUNT, scen.orders.size() / 10));
                std::cout << "Done (Total Avg: " << static_cast<uint64_t>(stats.totalStats.mean) << ")\n";

                allResults.emplace_back(implName, scen.name, stats);
            }
            std::cout << "\n";
        }
    }

    BenchmarkFormatter::printResultsTable(allResults);

    std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string csvFilename = "../../results/benchmark_breakdown_" + timestamp + ".csv";
    BenchmarkFormatter::exportResults(csvFilename, allResults);

    return 0;
}
