#pragma once

#include "../utils/LockFreeQueue.hpp"
#include "MetricsCollector.hpp"
#include "IOrderBook.hpp"
#include "../core/Order.hpp"
#include <atomic>

namespace hft
{

    class MatchingEngine
    {
    public:
        // Constructor takes input queue and order book reference (dependency injection)
        MatchingEngine(LockFreeQueue<Order, 1024> &inputQueue, IOrderBook &orderBook);

        // Main loop for the worker thread
        void run(std::atomic<bool> &running);

        // Core processing method (kept for testing/direct access)
        void processOrder(const Order &order);

        // Accessors for verification
        const MetricsCollector &getMetrics() const;

        // Const / Non-const for write/read and read only access
        IOrderBook &getOrderBook();
        const IOrderBook &getOrderBook() const;

    private:
        LockFreeQueue<Order, 1024> &inputQueue_;
        IOrderBook &orderBook_; // Reference to injected order book
        MetricsCollector metrics_;
    };

} // namespace hft
