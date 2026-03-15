#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace hft
{

struct LatencyStats
{
    uint64_t min;
    uint64_t max;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t p999;
    double mean;
    double stddev;
    size_t sampleCount;
};

class MetricsCollector
{
  public:
    MetricsCollector() : orderCount_(0), tradeCount_(0)
    {
        samples_.reserve(10000000); // Pre-allocate for 10M samples
    }

    void recordLatency(uint64_t cycles)
    {
        samples_.push_back(cycles);
    }

    void recordNetworkLatency(uint64_t cycles)
    {
        networkSamples_.push_back(cycles);
    }

    void recordEngineLatency(uint64_t cycles)
    {
        engineSamples_.push_back(cycles);
    }

    void incrementOrders()
    {
        orderCount_.fetch_add(1, std::memory_order_relaxed);
    }

    void incrementTrades(uint64_t count)
    {
        tradeCount_.fetch_add(count, std::memory_order_relaxed);
    }

    uint64_t getOrderCount() const
    {
        return orderCount_.load(std::memory_order_relaxed);
    }

    uint64_t getTradeCount() const
    {
        return tradeCount_.load(std::memory_order_relaxed);
    }

    LatencyStats getStats() const
    {
        return calculateStats(samples_);
    }

    LatencyStats getNetworkStats() const
    {
        return calculateStats(networkSamples_);
    }

    LatencyStats getEngineStats() const
    {
        return calculateStats(engineSamples_);
    }

    void printReport() const
    {
        if (samples_.empty())
        {
            std::cout << "No latency samples recorded\n";
            return;
        }

        LatencyStats stats = getStats();
        LatencyStats netStats = getNetworkStats();
        LatencyStats engStats = getEngineStats();

        std::cout << "\n=== Latency Statistics (Nanoseconds) ===\n";
        std::cout << "Sample Count:  " << stats.sampleCount << "\n";
        std::cout << "Total Mean:    " << static_cast<uint64_t>(stats.mean) << " ns\n";
        std::cout << "Network Mean:  " << static_cast<uint64_t>(netStats.mean) << " ns\n";
        std::cout << "Engine Mean:   " << static_cast<uint64_t>(engStats.mean) << " ns\n";
        std::cout << "P99 (Total):   " << stats.p99 << " ns\n";

        std::cout << "\n=== Order/Trade Statistics ===\n";
        std::cout << "Total Orders:  " << getOrderCount() << "\n";
        std::cout << "Total Trades:  " << getTradeCount() << "\n";
    }

    void exportCSV(const std::string &filename) const
    {
        std::ofstream file(filename);
        file << "total_ns,network_ns,engine_ns\n";
        size_t count = samples_.size();
        for (size_t i = 0; i < count; ++i)
        {
            file << samples_[i] << "," 
                 << (i < networkSamples_.size() ? networkSamples_[i] : 0) << ","
                 << (i < engineSamples_.size() ? engineSamples_[i] : 0) << "\n";
        }
    }

    void exportSummaryCSV(const std::string &filename) const
    {
        LatencyStats stats = getStats();

        std::ofstream file(filename);
        file << "metric,value\n";
        file << "sample_count," << stats.sampleCount << "\n";
        file << "mean," << stats.mean << "\n";
        file << "p99," << stats.p99 << "\n";
        file << "order_count," << getOrderCount() << "\n";
        file << "trade_count," << getTradeCount() << "\n";
    }

    size_t getSampleCount() const
    {
        return samples_.size();
    }

  private:
    LatencyStats calculateStats(const std::vector<uint64_t>& samples) const
    {
        if (samples.empty())
        {
            return {0, 0, 0, 0, 0, 0, 0.0, 0.0, 0};
        }

        std::vector<uint64_t> sortedSamples = samples;
        std::sort(sortedSamples.begin(), sortedSamples.end());

        size_t size = sortedSamples.size();
        double sum = std::accumulate(sortedSamples.begin(), sortedSamples.end(), 0.0);
        double mean = sum / size;

        double variance = 0.0;
        for (uint64_t sample : sortedSamples)
        {
            double diff = sample - mean;
            variance += diff * diff;
        }
        double stddev = std::sqrt(variance / size);

        auto getPercentile = [&](double percentile) -> uint64_t
        {
            size_t index = static_cast<size_t>(size * percentile);
            if (index >= size)
                index = size - 1;
            return sortedSamples[index];
        };

        return {
            sortedSamples.front(), // min
            sortedSamples.back(),  // max
            getPercentile(0.50),   // p50
            getPercentile(0.95),   // p95
            getPercentile(0.99),   // p99
            getPercentile(0.999),  // p999
            mean,                  // mean
            stddev,                // stddev
            size                   // sampleCount
        };
    }

    std::vector<uint64_t> samples_;
    std::vector<uint64_t> networkSamples_;
    std::vector<uint64_t> engineSamples_;
    std::atomic<uint64_t> orderCount_;
    std::atomic<uint64_t> tradeCount_;
};

} // namespace hft
