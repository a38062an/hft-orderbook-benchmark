#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include "modules/csv_order_generator.hpp"
#include "modules/mock_client.hpp"
#include "modules/order_book_benchmark.hpp"
#include "modules/order_generator.hpp"

#include "core/order.hpp"
#include "core/order_book_factory.hpp"
#include "utils/thread_pinning.hpp"

using namespace hft;

void printUsage()
{
    std::cout << "Usage: orderbook_benchmark [options]\n"
              << "Options:\n"
              << "  --mode <direct|gateway>  (default: direct)\n"
              << "  --book <map|array|vector|hybrid|pool|all> (default: map)\n"
              << "  --scenario <name|all>    (default: mixed)\n"
              << "  --csv <filename>         (optional: load orders from CSV)\n"
              << "  --orders <count>         (default: 10000, if no CSV)\n"
              << "  --port <number>          (default: 12345, for gateway mode)\n"
              << "  --runs <count>           (default: 1)\n"
              << "  --csv_out <filename>     (default: results/results.csv)\n"
              << "  --list_books             (list all supported order book types and exit)\n";
}

struct BenchmarkResult
{
    std::string mode;
    std::string book;
    std::string scenario;
    double mean;
    uint64_t p99;
    uint64_t max;
    double throughput;
    
    double dirInsertMean;
    double dirCancelMean;
    double dirLookupMean;
    double dirMatchMean;

    double serverMean;
    uint64_t serverP99;
    uint64_t serverMax;
    double serverNetMean;
    double serverQueMean;
    double serverEngMean;
};

std::vector<BenchmarkResult> loadExistingResults(const std::string &filename)
{
    std::vector<BenchmarkResult> results;
    std::ifstream file(filename);
    if (!file.is_open()) 
    {
        return results;
    }

    std::string line;
    std::getline(file, line); // Skip header

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string mode, book, scenario, lat_s, p99_s, max_s, thru_s, net_s, que_s, eng_s, ins_s, can_s, lkp_s, mtc_s;
        
        std::getline(ss, mode, ',');
        std::getline(ss, book, ',');
        std::getline(ss, scenario, ',');
        std::getline(ss, lat_s, ',');
        std::getline(ss, p99_s, ',');
        std::getline(ss, max_s, ',');
        std::getline(ss, thru_s, ',');
        std::getline(ss, net_s, ',');
        std::getline(ss, que_s, ',');
        std::getline(ss, eng_s, ',');
        std::getline(ss, ins_s, ',');
        std::getline(ss, can_s, ',');
        std::getline(ss, lkp_s, ',');
        std::getline(ss, mtc_s, ',');

        try 
        {
            BenchmarkResult res;
            res.mode = mode; 
            res.book = book; 
            res.scenario = scenario;
            
            double mean = std::stod(lat_s);
            uint64_t p99 = std::stoull(p99_s);
            uint64_t max = std::stoull(max_s);
            res.throughput = std::stod(thru_s);

            if (mode == "gateway") 
            {
                res.serverMean = mean; 
                res.serverP99 = p99; 
                res.serverMax = max;
                res.mean = 0; res.p99 = 0; res.max = 0;
                res.dirInsertMean = 0; res.dirCancelMean = 0; res.dirLookupMean = 0; res.dirMatchMean = 0;
                
                if (!net_s.empty()) 
                {
                    res.serverNetMean = std::stod(net_s);
                }
                if (!que_s.empty()) 
                {
                    res.serverQueMean = std::stod(que_s);
                }
                if (!eng_s.empty()) 
                {
                    res.serverEngMean = std::stod(eng_s);
                }
            } 
            else 
            {
                res.mean = mean; 
                res.p99 = p99; 
                res.max = max;
                res.serverMean = 0; res.serverP99 = 0; res.serverMax = 0;
                res.serverNetMean = 0; res.serverQueMean = 0; res.serverEngMean = 0;
                
                if (!ins_s.empty()) res.dirInsertMean = std::stod(ins_s);
                if (!can_s.empty()) res.dirCancelMean = std::stod(can_s);
                if (!lkp_s.empty()) res.dirLookupMean = std::stod(lkp_s);
                if (!mtc_s.empty()) res.dirMatchMean = std::stod(mtc_s);
            }
            results.push_back(res);
        } 
        catch (...) {}
    }
    return results;
}

void saveResults(const std::string &filename, const std::vector<BenchmarkResult> &results)
{
    std::ofstream outFile(filename, std::ios::trunc);
    if (!outFile.is_open()) 
    {
        return;
    }

    // Updated header to include Network, Engine, and granular Direct latency
    outFile << "Mode,Book,Scenario,Latency_ns,P99_ns,Max_ns,Throughput,Network_ns,Queue_ns,Engine_ns,Insert_ns,Cancel_ns,Lookup_ns,Match_ns\n";
    for (const auto &res : results)
    {
        double meanLat = (res.mode == "gateway") ? res.serverMean : res.mean;
        uint64_t p99Lat = (res.mode == "gateway") ? res.serverP99 : res.p99;
        uint64_t maxLat = (res.mode == "gateway") ? res.serverMax : res.max;

        outFile << res.mode << "," << res.book << "," << res.scenario << ","
                << std::fixed << std::setprecision(2) << meanLat << ","
                << p99Lat << "," << maxLat << "," << res.throughput << ","
                << res.serverNetMean << "," << res.serverQueMean << "," << res.serverEngMean << ","
                << res.dirInsertMean << "," << res.dirCancelMean << "," << res.dirLookupMean << "," << res.dirMatchMean << "\n";
    }
}

void upsertResult(std::vector<BenchmarkResult> &results, const BenchmarkResult &newRes)
{
    results.erase(std::remove_if(results.begin(), results.end(),
                                 [&](const BenchmarkResult &r) {
                                     return r.mode == newRes.mode && r.book == newRes.book && r.scenario == newRes.scenario;
                                 }),
                  results.end());
    results.push_back(newRes);
}

void cleanupDuplicates(std::vector<BenchmarkResult> &results)
{
    std::reverse(results.begin(), results.end());
    std::vector<BenchmarkResult> uniqueResults;
    for (const auto &res : results)
    {
        bool duplicate = false;
        for (const auto &u : uniqueResults)
        {
            if (u.mode == res.mode && u.scenario == res.scenario && u.book == res.book)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            uniqueResults.push_back(res);
    }
    results = uniqueResults;
    std::reverse(results.begin(), results.end());
}

void printSummaryTable(const std::vector<BenchmarkResult> &allResults, const std::string &csvOut)
{
    if (allResults.empty()) return;

    // Sort for a professional dissertation-ready table
    auto sortedResults = allResults;
    std::sort(sortedResults.begin(), sortedResults.end(), [](const BenchmarkResult &a, const BenchmarkResult &b) {
        if (a.mode != b.mode) return a.mode < b.mode;
        if (a.scenario != b.scenario) return a.scenario < b.scenario;
        return a.book < b.book;
    });

    std::cout << "\n" << std::string(187, '=') << "\n";
    std::cout << "GLOBAL PERFORMANCE SUMMARY (all recorded runs)\n";
    std::cout << std::string(187, '=') << "\n";
    std::cout << std::left << std::setw(10) << "Mode" << std::setw(20) << "Scenario" << std::setw(12) << "Book";
    std::cout << std::right << std::setw(13) << "Latency(ns)" << std::setw(12) << "P99(ns)" << std::setw(12) << "Max(ns)" << std::setw(15) << "Throughput" 
              << std::setw(12) << "Net(ns)" << std::setw(12) << "Que(ns)" << std::setw(12) << "Eng(ns)"
              << std::setw(12) << "Ins(ns)" << std::setw(12) << "Can(ns)" << std::setw(12) << "Lkp(ns)" << std::setw(15) << "Match(ns)";
    std::cout << "\n" << std::string(187, '-') << "\n";

    std::string lastModeScenario = "";
    for (const auto &res : sortedResults)
    {
        std::string currentKey = res.mode + res.scenario;
        if (!lastModeScenario.empty() && currentKey != lastModeScenario)
        {
            std::cout << std::string(187, '-') << "\n";
        }
        
        std::cout << std::left << std::setw(10) << res.mode << std::setw(20) << res.scenario << std::setw(12) << res.book;
        
        double displayMean = (res.mode == "gateway") ? res.serverMean : res.mean;
        uint64_t displayP99 = (res.mode == "gateway") ? res.serverP99 : res.p99;
        uint64_t displayMax = (res.mode == "gateway") ? res.serverMax : res.max;

        std::cout << std::right << std::fixed << std::setprecision(2) << std::setw(13) << displayMean
                  << std::setw(12) << displayP99 << std::setw(12) << displayMax 
                  << std::fixed << std::setprecision(0) << std::setw(15) << res.throughput;
                  
        if (res.mode == "gateway")
        {
            std::cout << std::setw(12) << std::fixed << std::setprecision(2) << res.serverNetMean
                      << std::setw(12) << res.serverQueMean
                      << std::setw(12) << res.serverEngMean
                      << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(15) << "-";
        }
        else
        {
            std::cout << std::setw(12) << "-" << std::setw(12) << "-" << std::setw(12) << "-"
                      << std::setw(12) << std::fixed << std::setprecision(2) << res.dirInsertMean
                      << std::setw(12) << res.dirCancelMean
                      << std::setw(12) << res.dirLookupMean
                      << std::setw(15) << res.dirMatchMean;
        }
        std::cout << "\n";
        lastModeScenario = currentKey;
    }
    std::cout << std::string(187, '=') << "\n";
    std::cout << "[Note] Latency = Pure Algorithmic Time (Direct) or End-to-End System Time (Gateway)\n";
    if (!csvOut.empty())
    {
        std::cout << "Results saved to: " << csvOut << "\n\n";
    }
}

void runDirectBenchmark(const std::string &currentBook, const std::string &scenario, 
                        const std::vector<Order> &orders, int runs, 
                        std::vector<BenchmarkResult> &allResults)
{
    auto book = OrderBookFactory::create(currentBook);
    OrderBookBenchmark benchmark(currentBook, std::move(book));

    std::cout << "Running direct benchmark for " << currentBook << " (" << runs << " runs)...\n";
    
    double avgMean = 0, avgP99 = 0, avgMax = 0, avgThroughput = 0;
    double avgIns = 0, avgCan = 0, avgLkp = 0, avgMtc = 0;
    for (int r = 0; r < runs; ++r)
    {


        // --- Start of Direct Benchmark ---
        auto start = std::chrono::high_resolution_clock::now();
        auto stats = benchmark.runScenario(orders, 1, orders.size() / 10);
        auto end = std::chrono::high_resolution_clock::now();
        // --- End of Direct Benchmark ---


        avgMean += stats.totalStats.mean;
        avgP99 += stats.totalStats.p99;
        avgMax += stats.totalStats.max;
        
        avgIns += stats.insertStats.mean;
        avgCan += stats.cancelStats.mean;
        avgLkp += stats.lookupStats.mean;
        avgMtc += stats.matchStats.mean;

        auto durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        if (durationNs > 0) 
        {
            avgThroughput += (orders.size() * 1e9 / durationNs);
        }
    }
    avgMean /= runs; 
    avgP99 /= runs; 
    avgMax /= runs; 
    avgThroughput /= runs;
    avgIns /= runs;
    avgCan /= runs;
    avgLkp /= runs;
    avgMtc /= runs;

    upsertResult(allResults, {"direct", currentBook, scenario, avgMean, (uint64_t)avgP99, (uint64_t)avgMax, avgThroughput, 
                              avgIns, avgCan, avgLkp, avgMtc, 
                              0.0, 0ULL, 0ULL, 0.0, 0.0, 0.0});
    std::cout << "  Mean Latency:   " << std::fixed << std::setprecision(2) << avgMean << " ns\n";
}

void runGatewayBenchmark(const std::string &currentBook, const std::string &scenario, 
                         const std::vector<Order> &orders, int runs, int port,
                         std::vector<BenchmarkResult> &allResults)
{
    std::cout << "Running gateway benchmark for " << currentBook << " (" << runs << " runs)...\n";
    
    double avgThroughput = 0, serverMean = 0, serverNetMean = 0, serverQueMean = 0, serverEngMean = 0;
    unsigned long long serverP99 = 0, serverMax = 0;

    for (int r = 0; r < runs; ++r)
    {
        MockClient client("127.0.0.1", port);
        if (!client.connect()) 
        {
            std::cerr << "Failed to connect to gateway at 127.0.0.1:" << port << "\n";
            return;
        }

        // Time startTotal and endTotal used to calculate throughput
        auto startTotal = std::chrono::high_resolution_clock::now();
        for (const auto &order : orders) 
        {
            // order.sendTimeStamp is set here  (Beginning time stamp for E2E latency)
            client.sendOrder(order);
        }

        // Stats only get sent back once all orders have been matched
        auto sStats = client.requestServerStats(orders.size());
        auto endTotal = std::chrono::high_resolution_clock::now();

        auto durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTotal - startTotal).count();
        if (durationNs > 0) 
        {
            avgThroughput += (orders.size() * 1e9 / durationNs);
        }

        if (sStats) 
        {
            serverMean += sStats->mean;
            serverP99 = std::max(serverP99, (unsigned long long)sStats->p99);
            serverMax = std::max(serverMax, (unsigned long long)sStats->max);
            serverNetMean += sStats->netMean;
            serverQueMean += sStats->queMean;
            serverEngMean += sStats->engMean;
        }
        client.disconnect();
    }

    avgThroughput /= runs; 
    serverMean /= runs;
    serverNetMean /= runs;
    serverQueMean /= runs;
    serverEngMean /= runs;
    
    // We only care about server-side metrics for Gateway E2E results
    upsertResult(allResults, {"gateway", currentBook, scenario, 0.0 /* client avg */, 0ULL /* client p99 */, 0ULL /* client max */, 
                             avgThroughput, 
                             0.0, 0.0, 0.0, 0.0, /* Direct mode specific */
                             serverMean, serverP99, serverMax, serverNetMean, serverQueMean, serverEngMean});
}

int main(int argc, char *argv[])
{
    std::string mode = "direct";
    std::string bookType = "map";
    std::string scenarioName = "mixed";
    std::string csvFile = "";
    std::string csvOut = "results/results.csv";
    size_t orderCount = 10000;
    int port = 12345;
    int runs = 1; // Default to 1 run

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
            mode = argv[++i];
        else if (arg == "--book" && i + 1 < argc)
            bookType = argv[++i];
        else if (arg == "--scenario" && i + 1 < argc)
            scenarioName = argv[++i];
        else if (arg == "--csv" && i + 1 < argc)
            csvFile = argv[++i];
        else if (arg == "--csv_out" && i + 1 < argc)
            csvOut = argv[++i];
        else if (arg == "--runs" && i + 1 < argc)
        {
            try
            {
                runs = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --runs: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--orders" && i + 1 < argc)
        {
            try
            {
                orderCount = std::stoul(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --orders: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            try
            {
                port = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Error: Invalid number for --port: " << argv[i] << "\n";
                printUsage();
                return 1;
            }
        }
        else if (arg == "--list_books")
        {
            auto types = OrderBookFactory::getSupportedTypes();
            for (size_t i = 0; i < types.size(); ++i)
                std::cout << types[i] << (i == types.size() - 1 ? "" : " ");
            std::cout << "\n";
            return 0;
        }
        else if (arg == "--list_scenarios")
        {
            auto scenarios = OrderGenerator::getSupportedScenarios();
            for (size_t i = 0; i < scenarios.size(); ++i)
                std::cout << scenarios[i] << (i == scenarios.size() - 1 ? "" : " ");
            std::cout << "\n";
            return 0;
        }
        else if (arg == "--help")
        {
            printUsage();
            return 0;
        }
        else
        {
            std::cerr << "Error: Unknown or incomplete option: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    if (hft::pinToCore(0))
        std::cout << "Thread successfully pinned to core 0\n";

    std::cout << "HFT OrderBook Benchmark (" << mode << " mode, " << bookType << " book)\n";
    std::cout << "==========================================\n";

    std::vector<std::string> targetScenarios;
    if (scenarioName == "all")
    {
        targetScenarios = OrderGenerator::getSupportedScenarios();
    }
    else
    {
        targetScenarios = {scenarioName};
    }

    std::vector<std::string> targetBooks;
    if (bookType == "all")
    {
        targetBooks = OrderBookFactory::getSupportedTypes();
    }
    else
    {
        targetBooks = {bookType};
    }

    OrderGenerator generator(42);
    
    // Load existing results to show a "Global Summary"
    std::vector<BenchmarkResult> allResults = loadExistingResults(csvOut);
    cleanupDuplicates(allResults);

    for (const auto &currentScenario : targetScenarios)
    {
        std::vector<Order> orders;
        if (!csvFile.empty())
        {
            if (scenarioName == "all")
            {
                 std::cout << "Skipping scenario loop: Using fixed CSV file instead.\n";
                 orders = CSVOrderGenerator::parse(csvFile);
                 // We run it once for the CSV
                 if (currentScenario != targetScenarios[0]) continue; 
            }
            else
            {
                orders = CSVOrderGenerator::parse(csvFile);
            }
        }
        else
        {
            std::cout << "\nScenario: " << currentScenario << "\n";
            std::cout << "------------------------------------------\n";
            orders = generator.generateScenario(currentScenario, orderCount);
        }

        for (const auto &currentBook : targetBooks)
        {
            if (mode == "direct")
                runDirectBenchmark(currentBook, currentScenario, orders, runs, allResults);
            else if (mode == "gateway")
                runGatewayBenchmark(currentBook, currentScenario, orders, runs, port, allResults);
        }
    }

    if (!csvOut.empty())
        saveResults(csvOut, allResults);

    printSummaryTable(allResults, csvOut);

    return 0;
}
