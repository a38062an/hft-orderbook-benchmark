#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>
#include <cmath>
#include <atomic>

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
            if (samples_.empty())
            {
                return {0, 0, 0, 0, 0, 0, 0.0, 0.0, 0};
            }

            std::vector<uint64_t> sortedSamples = samples_;
            std::sort(sortedSamples.begin(), sortedSamples.end());

            size_t size = sortedSamples.size();

            // Calculate mean
            double sum = std::accumulate(sortedSamples.begin(), sortedSamples.end(), 0.0);
            double mean = sum / size;

            // Calculate standard deviation
            double variance = 0.0;
            for (uint64_t sample : sortedSamples)
            {
                double diff = sample - mean;
                variance += diff * diff;
            }
            double stddev = std::sqrt(variance / size);

            // Calculate percentiles
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

        void printReport() const
        {
            if (samples_.empty())
            {
                std::cout << "No latency samples recorded\n";
                return;
            }

            LatencyStats stats = getStats();

            std::cout << "\n=== Latency Statistics (CPU Cycles) ===\n";
            std::cout << "Sample Count:  " << stats.sampleCount << "\n";
            std::cout << "Min:           " << stats.min << " cycles\n";
            std::cout << "Max:           " << stats.max << " cycles\n";
            std::cout << "Mean:          " << static_cast<uint64_t>(stats.mean) << " cycles\n";
            std::cout << "Std Dev:       " << static_cast<uint64_t>(stats.stddev) << " cycles\n";
            std::cout << "P50 (median):  " << stats.p50 << " cycles\n";
            std::cout << "P95:           " << stats.p95 << " cycles\n";
            std::cout << "P99:           " << stats.p99 << " cycles\n";
            std::cout << "P99.9:         " << stats.p999 << " cycles\n";

            std::cout << "\n=== Order/Trade Statistics ===\n";
            std::cout << "Total Orders:  " << getOrderCount() << "\n";
            std::cout << "Total Trades:  " << getTradeCount() << "\n";
        }

        void exportCSV(const std::string &filename) const
        {
            std::ofstream file(filename);
            file << "latency_cycles\n";
            for (auto sample : samples_)
            {
                file << sample << "\n";
            }
        }

        void exportSummaryCSV(const std::string &filename) const
        {
            LatencyStats stats = getStats();

            std::ofstream file(filename);
            file << "metric,value\n";
            file << "sample_count," << stats.sampleCount << "\n";
            file << "min," << stats.min << "\n";
            file << "max," << stats.max << "\n";
            file << "mean," << stats.mean << "\n";
            file << "stddev," << stats.stddev << "\n";
            file << "p50," << stats.p50 << "\n";
            file << "p95," << stats.p95 << "\n";
            file << "p99," << stats.p99 << "\n";
            file << "p999," << stats.p999 << "\n";
            file << "order_count," << getOrderCount() << "\n";
            file << "trade_count," << getTradeCount() << "\n";
        }

        size_t getSampleCount() const
        {
            return samples_.size();
        }

    private:
        std::vector<uint64_t> samples_;
        std::atomic<uint64_t> orderCount_;
        std::atomic<uint64_t> tradeCount_;
    };

} // namespace hft
