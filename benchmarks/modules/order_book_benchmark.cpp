#include "order_book_benchmark.hpp"
#include "../src/utils/rdtsc.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace hft {

OperationBreakdown OrderBookBenchmark::runScenario(const std::vector<Order> &orders,
                                size_t readsPerOp,
                                size_t warmupCount)
{
    MetricsCollector insertMetrics;
    MetricsCollector cancelMetrics;
    MetricsCollector lookupMetrics;
    MetricsCollector matchMetrics;
    MetricsCollector totalMetrics;

    // Warmup phase
    for (size_t i = 0; i < warmupCount && i < orders.size(); ++i)
    {
        if (orders[i].quantity == 0) orderbook_->cancelOrder(orders[i].id);
        else orderbook_->addOrder(orders[i]);
        
        for(size_t r=0; r<readsPerOp; ++r) {
            (void)orderbook_->getBestBid();
            (void)orderbook_->getBestAsk();
        }
        orderbook_->match();
    }

    // Measurement phase
    for (size_t i = warmupCount; i < orders.size(); ++i)
    {
        uint64_t totalStart = rdtsc();

        // Measure Insert vs Cancel Separately
        // Note: The generator uses quantity == 0 as a flag to signify a Cancel order
        if (orders[i].quantity == 0) 
        {
            uint64_t cancelStart = rdtsc();
            orderbook_->cancelOrder(orders[i].id);
            uint64_t cancelEnd = rdtsc();
            cancelMetrics.recordLatency(cancelEnd - cancelStart);
        }
        else 
        {
            uint64_t insertStart = rdtsc();
            orderbook_->addOrder(orders[i]);
            uint64_t insertEnd = rdtsc();
            insertMetrics.recordLatency(insertEnd - insertStart);
        }

        // Measure Lookups (Read Heavy work)
        for (size_t r = 0; r < readsPerOp; ++r)
        {
            uint64_t lookupStart = rdtsc();
            volatile auto bestBid = orderbook_->getBestBid();
            volatile auto bestAsk = orderbook_->getBestAsk();
            (void)bestBid; (void)bestAsk;
            uint64_t lookupEnd = rdtsc();
            lookupMetrics.recordLatency(lookupEnd - lookupStart);
        }

        // Measure Match
        uint64_t matchStart = rdtsc();
        orderbook_->match();
        uint64_t matchEnd = rdtsc();
        matchMetrics.recordLatency(matchEnd - matchStart);

        uint64_t totalEnd = rdtsc();
        totalMetrics.recordLatency(totalEnd - totalStart);
    }

    return OperationBreakdown{
        insertMetrics.getStats(),
        cancelMetrics.getStats(),
        lookupMetrics.getStats(),
        matchMetrics.getStats(),
        totalMetrics.getStats()};
}

void BenchmarkFormatter::exportResults(const std::string &filename,
                    const std::vector<std::tuple<std::string, std::string, OperationBreakdown>> &results)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // Write CSV header
    file << "OrderBook,Scenario,OrderCount,SampleCount,"
            << "Total_Mean,Total_P99,"
            << "Insert_Mean,Insert_P99,"
            << "Cancel_Mean,Cancel_P99,"
            << "Lookup_Mean,Lookup_P99,"
            << "Match_Mean,Match_P99\n";

    // Write data rows
    for (const auto &[orderbook, scenario, stats] : results)
    {
        std::string orderCount = "100K"; 
        size_t pos = scenario.find("_");
        if (pos != std::string::npos) orderCount = scenario.substr(pos + 1);

        file << orderbook << ","
                << scenario << ","
                << orderCount << ","
                << stats.totalStats.sampleCount << ","
                << stats.totalStats.mean << "," << stats.totalStats.p99 << ","
                << stats.insertStats.mean << "," << stats.insertStats.p99 << ","
                << stats.cancelStats.mean << "," << stats.cancelStats.p99 << ","
                << stats.lookupStats.mean << "," << stats.lookupStats.p99 << ","
                << stats.matchStats.mean << "," << stats.matchStats.p99 << "\n";
    }

    file.close();
    std::cout << "Results exported to: " << filename << std::endl;
}

void BenchmarkFormatter::printResultsTable(std::vector<std::tuple<std::string, std::string, OperationBreakdown>> &results)
{
    std::sort(results.begin(), results.end(),
                [](const auto &a, const auto &b)
                {
                    if (std::get<1>(a) != std::get<1>(b)) return std::get<1>(a) < std::get<1>(b);
                    return std::get<0>(a) < std::get<0>(b);
                });

    std::cout << "\n" << std::string(140, '=') << "\n";
    std::cout << "ORDERBOOK BENCHMARK RESULTS (Cycles) - Breakdown [Insert | Cancel | Lookup | Match | Total]\n";
    std::cout << std::string(140, '=') << "\n\n";

    std::cout << std::left << std::setw(25) << "Scenario"
                << std::setw(18) << "OrderBook"
                << std::right 
                << std::setw(10) << "Ins(Avg)"
                << std::setw(10) << "Cxl(Avg)"
                << std::setw(10) << "Look(Avg)"
                << std::setw(10) << "Match(Avg)"
                << std::setw(12) << "Total(Avg)" 
                << std::setw(12) << "Total(P99)" << "\n";

    std::cout << std::string(140, '-') << "\n";

    std::string currentScenario = "";
    for (const auto &[orderbook, scenario, stats] : results)
    {
        if (scenario != currentScenario && !currentScenario.empty())
            std::cout << std::string(140, '-') << "\n";
        currentScenario = scenario;

        std::cout << std::left << std::setw(25) << scenario
                    << std::setw(18) << orderbook
                    << std::right << std::fixed << std::setprecision(0)
                    << std::setw(10) << stats.insertStats.mean
                    << std::setw(10) << stats.cancelStats.mean
                    << std::setw(10) << stats.lookupStats.mean
                    << std::setw(10) << stats.matchStats.mean
                    << std::setw(12) << stats.totalStats.mean
                    << std::setw(12) << stats.totalStats.p99 << "\n";
    }

    std::cout << std::string(140, '=') << "\n\n";
}

} // namespace hft
