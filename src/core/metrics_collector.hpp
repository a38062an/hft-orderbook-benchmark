#pragma once

#include <atomic>
#include <vector>
#include <cstdint>
#include <iostream>

namespace hft {

// RDTSC: Read Time-Stamp Counter
// Returns the number of CPU cycles since reset.
// Used for ultra-low latency measurements (~5ns overhead).
inline uint64_t rdtsc() 
{
#if defined(__aarch64__)
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
#else
    unsigned int lo, hi;
    __asm__ __volatile__ (
        "rdtsc"
        : "=a" (lo), "=d" (hi)
    );
    return ((uint64_t)hi << 32) | lo;
#endif
}

class MetricsCollector 
{
public:
    MetricsCollector() : orderCount_(0), tradeCount_(0) 
    {
        // Pre-allocate to avoid allocation during runtime
        latencies_.reserve(1000000); 
    }

    void recordLatency(uint64_t cycles) 
    {
        // In a real lock-free system, we'd use a ring buffer here.
        // For this scaffold, we'll just use a vector but be careful.
        // Since MatchingEngine is single-threaded, this is safe for now.
        if (latencies_.size() < latencies_.capacity())
        {
            latencies_.push_back(cycles);
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

private:
    std::atomic<uint64_t> orderCount_;
    std::atomic<uint64_t> tradeCount_;
    std::vector<uint64_t> latencies_;
};

} // namespace hft
