#include "matching_engine.hpp"
#include "utils/rdtsc.hpp"

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
    // 1. Start Engine Timer
    uint64_t engineStart = getCurrentTimeNs();

    // 2. Insert into Order Book
    orderBook_.addOrder(order);

    // 3. Match Orders
    std::vector<Trade> trades = orderBook_.match();

    // 4. Stop Engine Timer
    uint64_t engineEnd = getCurrentTimeNs();

    // 5. Calculate Decomposed Latencies
    uint64_t networkLat = 0;
    uint64_t queueLat = 0;
    uint64_t engineLat = engineEnd - engineStart;
    uint64_t totalLat = 0;

    // Use sendTimestamp (E2E) if available, otherwise receiveTimestamp (Wire-to-Match)
    if (order.sendTimestamp > 0)
    {
        totalLat = engineEnd - order.sendTimestamp;
        if (order.receiveTimestamp > 0)
        {
            networkLat = order.receiveTimestamp - order.sendTimestamp;
            queueLat = engineStart - order.receiveTimestamp;
        }
    }
    else if (order.receiveTimestamp > 0)
    {
        totalLat = engineEnd - order.receiveTimestamp;
        queueLat = engineStart - order.receiveTimestamp;
    }
    else
    {
        totalLat = engineLat; // Direct mode
    }

    // 6. Record Metrics
    metrics_.recordLatency(totalLat);
    if (networkLat > 0)
    {
        metrics_.recordNetworkLatency(networkLat);
    }
    if (queueLat > 0)
    {
        metrics_.recordQueueLatency(queueLat);
    }
    metrics_.recordEngineLatency(engineLat);

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
