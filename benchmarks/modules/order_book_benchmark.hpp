#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "core/Order.hpp"
#include "core/i_order_book.hpp"
#include "utils/metrics_collector.hpp"

namespace hft
{

/**
 * @brief Operation-level breakdown metrics
 */
struct OperationBreakdown
{
    LatencyStats insertStats;
    LatencyStats cancelStats;
    LatencyStats lookupStats;
    LatencyStats matchStats;
    LatencyStats totalStats;
};

/**
 * @brief Benchmark runner for a single orderbook implementation
 */
class OrderBookBenchmark
{
  public:
    OrderBookBenchmark(const std::string &name, std::unique_ptr<IOrderBook> orderbook)
        : name_(name), orderbook_(std::move(orderbook))
    {
    }

    OperationBreakdown runScenario(const std::vector<Order> &orders, size_t readsPerOp, size_t warmupCount = 1000);

    const std::string &getName() const
    {
        return name_;
    }

  private:
    std::string name_;
    std::unique_ptr<IOrderBook> orderbook_;
};

/**
 * @brief Formatter utilities
 */
class BenchmarkFormatter
{
  public:
    static void exportResults(const std::string &filename,
                              const std::vector<std::tuple<std::string, std::string, OperationBreakdown>> &results);

    static void printResultsTable(std::vector<std::tuple<std::string, std::string, OperationBreakdown>> &results);
};

} // namespace hft
