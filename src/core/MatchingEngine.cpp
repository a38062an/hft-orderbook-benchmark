#include "MatchingEngine.hpp"

namespace hft
{

    MatchingEngine::MatchingEngine(LockFreeQueue<Order, 1024> &inputQueue, IOrderBook &orderBook)
        : inputQueue_(inputQueue), orderBook_(orderBook)
    {
    }

    void MatchingEngine::run(std::atomic<bool> &running)
    {
        Order order;
        while (running.load(std::memory_order_relaxed))
        {
            // Busy wait / spin loop for lowest latency
            while (inputQueue_.pop(order))
            {
                processOrder(order);
            }
            // Optional: cpu_relax() or yield if we want to be nice,
            // but for HFT pinning we usually spin.
        }
    }

    void MatchingEngine::processOrder(const Order &order)
    {
        // 1. Start Timer
        uint64_t start = rdtsc();

        // 2. Insert into Order Book
        // Note: In a real engine, we'd check order type (New/Cancel/Modify)
        // For now, assuming everything is a New Order for the benchmark
        orderBook_.addOrder(order);

        // 3. Match Orders
        std::vector<Trade> trades = orderBook_.match();

        // 4. Stop Timer
        uint64_t end = rdtsc();

        // 5. Record Metrics
        metrics_.recordLatency(end - start);
        metrics_.incrementOrders();
        metrics_.incrementTrades(trades.size());
    }

    const MetricsCollector &MatchingEngine::getMetrics() const
    {
        return metrics_;
    }

    IOrderBook &MatchingEngine::getOrderBook()
    {
        return orderBook_;
    }

    const IOrderBook &MatchingEngine::getOrderBook() const
    {
        return orderBook_;
    }

} // namespace hft
