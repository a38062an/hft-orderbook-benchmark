#pragma once

#include "../utils/LockFreeQueue.hpp"
#include "MetricsCollector.hpp"
#include "../orderbooks/MapOrderBook.hpp"
#include "../core/Order.hpp"
#include <atomic>

namespace hft {

class MatchingEngine 
{
public:
    // Constructor now takes a reference to the input queue
    explicit MatchingEngine(LockFreeQueue<Order, 1024>& inputQueue);

    // Main loop for the worker thread
    void run(std::atomic<bool>& running);

    // Core processing method (kept for testing/direct access)
    void processOrder(const Order& order);

    // Accessors for verification
    const MetricsCollector& getMetrics() const;
    const MapOrderBook& getOrderBook() const;

private:
    LockFreeQueue<Order, 1024>& inputQueue_;
    MapOrderBook orderBook_;
    MetricsCollector metrics_;
};

} // namespace hft
