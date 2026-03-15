#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace hft
{

struct LatencyStats
{
    double mean;
    uint64_t p99;
    uint64_t max;
};

class MetricsCollector
{
  public:
    MetricsCollector() : orderCount_(0), tradeCount_(0)
    {
        // Pre-allocate to avoid allocation during runtime
        latencies_.reserve(1000000);
        networkLatencies_.reserve(1000000);
        engineLatencies_.reserve(1000000);
        queueLatencies_.reserve(1000000);
    }

    void recordLatency(uint64_t cycles)
    {
        if (latencies_.size() < latencies_.capacity())
        {
            latencies_.push_back(cycles);
        }
    }

    void recordNetworkLatency(uint64_t cycles)
    {
        if (networkLatencies_.size() < networkLatencies_.capacity())
        {
            networkLatencies_.push_back(cycles);
        }
    }

    void recordEngineLatency(uint64_t cycles)
    {
        if (engineLatencies_.size() < engineLatencies_.capacity())
        {
            engineLatencies_.push_back(cycles);
        }
    }

    void recordQueueLatency(uint64_t cycles)
    {
        if (queueLatencies_.size() < queueLatencies_.capacity())
        {
            queueLatencies_.push_back(cycles);
        }
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
        return calculateStats(latencies_);
    }

    LatencyStats getNetworkStats() const
    {
        return calculateStats(networkLatencies_);
    }

    LatencyStats getEngineStats() const
    {
        return calculateStats(engineLatencies_);
    }

    LatencyStats getQueueStats() const
    {
        return calculateStats(queueLatencies_);
    }

  private:
    LatencyStats calculateStats(const std::vector<uint64_t> &samples) const
    {
        if (samples.empty())
        {
            return {0.0, 0, 0};
        }

        std::vector<uint64_t> sorted = samples;
        std::sort(sorted.begin(), sorted.end());

        double sum = 0;
        for (auto l : sorted)
        {
            sum += l;
        }

        LatencyStats stats;
        stats.mean = sum / sorted.size();
        stats.max = sorted.back();
        stats.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
        return stats;
    }

    std::atomic<uint64_t> orderCount_;
    std::atomic<uint64_t> tradeCount_;
    std::vector<uint64_t> latencies_;
    std::vector<uint64_t> networkLatencies_;
    std::vector<uint64_t> engineLatencies_;
    std::vector<uint64_t> queueLatencies_;
};

} // namespace hft
