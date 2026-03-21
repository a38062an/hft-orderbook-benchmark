#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "core/matching_engine.hpp"

namespace hft
{
namespace
{

class StubOrderBook : public IOrderBook
{
  public:
    void addOrder(const Order &order) override
    {
        lastOrder = order;
        ++addCalls;
    }

    void cancelOrder(OrderId) override
    {
    }

    void modifyOrder(OrderId, Quantity) override
    {
    }

    std::vector<Trade> match() override
    {
        ++matchCalls;
        return tradesToReturn;
    }

    std::size_t getOrderCount() const override
    {
        return 0;
    }

    Price getBestBid() const override
    {
        return 0;
    }

    Price getBestAsk() const override
    {
        return std::numeric_limits<Price>::max();
    }

    int addCalls = 0;
    int matchCalls = 0;
    Order lastOrder{};
    std::vector<Trade> tradesToReturn;
};

TEST(MatchingEngineTest, ProcessOrderInvokesBookAndUpdatesCounters)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;
    book.tradesToReturn = {{1, 2, 130, 5}, {3, 4, 131, 2}};

    MatchingEngine engine(queue, book);
    Order in{11, 130, 10, Side::Buy, OrderType::Limit, 0, 0, 0};

    engine.processOrder(in);

    EXPECT_EQ(book.addCalls, 1);
    EXPECT_EQ(book.matchCalls, 1);
    EXPECT_EQ(book.lastOrder.id, 11u);

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_EQ(engine.getMetrics().getTradeCount(), 2u);
}

TEST(MatchingEngineTest, ProcessOrderCapturesLatencySample)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;

    MatchingEngine engine(queue, book);
    Order in{22, 131, 10, Side::Sell, OrderType::Limit, 0, 0, 0};

    engine.processOrder(in);

    auto stats = engine.getMetrics().getStats();
    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_GE(stats.max, stats.p99);
    EXPECT_GE(stats.p99, 0u);
}

TEST(MatchingEngineTest, ProcessOrderWithSendAndReceiveRecordsDecomposedStats)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;

    MatchingEngine engine(queue, book);
    Order in{33, 132, 10, Side::Buy, OrderType::Limit, 0, 10, 1};

    engine.processOrder(in);

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_GT(engine.getMetrics().getNetworkStats().max, 0u);
    EXPECT_GT(engine.getMetrics().getQueueStats().max, 0u);
}

TEST(MatchingEngineTest, ProcessOrderWithOnlyReceiveTimestampRecordsQueue)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;

    MatchingEngine engine(queue, book);
    Order in{44, 133, 10, Side::Sell, OrderType::Limit, 0, 1, 0};

    engine.processOrder(in);

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_GT(engine.getMetrics().getQueueStats().max, 0u);
}

TEST(MatchingEngineTest, ProcessOrderWithOnlySendTimestampSkipsNetworkBreakdown)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;

    MatchingEngine engine(queue, book);
    Order in{45, 133, 10, Side::Sell, OrderType::Limit, 0, 0, 1};

    engine.processOrder(in);

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_EQ(engine.getMetrics().getNetworkStats().max, 0u);
    EXPECT_EQ(engine.getMetrics().getQueueStats().max, 0u);
}

TEST(MatchingEngineTest, RunConsumesQueueItems)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;
    MatchingEngine engine(queue, book);

    std::atomic<bool> running{true};
    std::thread t([&]() { engine.run(running); });

    Order in{55, 140, 2, Side::Buy, OrderType::Limit, 0, 0, 0};
    ASSERT_TRUE(queue.push(in));

    for (int i = 0; i < 1000 && engine.getMetrics().getOrderCount() == 0; ++i)
    {
        std::this_thread::yield();
    }

    running.store(false, std::memory_order_relaxed);
    t.join();

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
}

TEST(MatchingEngineTest, RunDrainsQueuedItemsAfterShutdown)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;
    MatchingEngine engine(queue, book);

    Order in{56, 141, 3, Side::Sell, OrderType::Limit, 0, 0, 0};
    ASSERT_TRUE(queue.push(in));

    std::atomic<bool> running{false};
    engine.run(running);

    EXPECT_EQ(book.addCalls, 1);
    EXPECT_EQ(book.matchCalls, 1);
    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
}

TEST(MatchingEngineTest, OrderBookGettersReturnBoundInstance)
{
    LockFreeQueue<Order, 1024> queue;
    StubOrderBook book;
    MatchingEngine engine(queue, book);

    IOrderBook &mutableRef = engine.getOrderBook();
    const MatchingEngine &constEngine = engine;
    const IOrderBook &constRef = constEngine.getOrderBook();

    EXPECT_EQ(&mutableRef, &book);
    EXPECT_EQ(&constRef, &book);
}

} // namespace
} // namespace hft
