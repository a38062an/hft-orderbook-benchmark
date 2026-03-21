#include <gtest/gtest.h>

#include "orderbooks/hybrid_order_book.hpp"

namespace hft
{
namespace
{

Order mk(OrderId id, Price price, Quantity qty, Side side)
{
    return Order{id, price, qty, side, OrderType::Limit, 0, 0, 0};
}

TEST(HybridOrderBookTest, EmptyBookBestPrices)
{
    HybridOrderBook book;
    EXPECT_EQ(book.getBestBid(), 0u);
    EXPECT_EQ(book.getBestAsk(), std::numeric_limits<Price>::max());
}

TEST(HybridOrderBookTest, ColdBidBecomesBestWhenHotCancelled)
{
    HybridOrderBook book(1);

    book.addOrder(mk(1, 100, 1, Side::Buy));
    book.addOrder(mk(2, 90, 1, Side::Buy));

    EXPECT_EQ(book.getBestBid(), 100u);

    book.cancelOrder(1);
    EXPECT_EQ(book.getBestBid(), 90u);
}

TEST(HybridOrderBookTest, DemotionAndPromotionPathsOnBothSides)
{
    HybridOrderBook book(1);

    // Trigger buy-side demotion path.
    book.addOrder(mk(10, 100, 1, Side::Buy));
    book.addOrder(mk(11, 101, 1, Side::Buy));

    // Trigger sell-side demotion path.
    book.addOrder(mk(20, 110, 1, Side::Sell));
    book.addOrder(mk(21, 109, 1, Side::Sell));

    // Make sure prices are still coherent after internal hot/cold moves.
    EXPECT_GE(book.getBestBid(), 100u);
    EXPECT_LE(book.getBestAsk(), 110u);

    // Force promotion-from-cold paths during match by emptying hot top levels first.
    book.cancelOrder(11);
    book.cancelOrder(21);

    // Add a deterministic crossing pair so match is guaranteed.
    book.addOrder(mk(30, 108, 1, Side::Buy));
    book.addOrder(mk(31, 108, 1, Side::Sell));

    auto trades = book.match();
    EXPECT_FALSE(trades.empty());
}

TEST(HybridOrderBookTest, NoOverlapProducesNoTrades)
{
    HybridOrderBook book(1);
    book.addOrder(mk(31, 100, 2, Side::Buy));
    book.addOrder(mk(32, 105, 2, Side::Sell));

    auto trades = book.match();
    EXPECT_TRUE(trades.empty());
}

TEST(HybridOrderBookTest, ModifyZeroCancelsOrder)
{
    HybridOrderBook book;
    book.addOrder(mk(40, 100, 2, Side::Buy));

    EXPECT_EQ(book.getOrderCount(), 1u);
    book.modifyOrder(40, 0);
    EXPECT_EQ(book.getOrderCount(), 0u);
}

TEST(HybridOrderBookTest, ColdLevelInsertAndCancelPaths)
{
    HybridOrderBook book(1);

    // Seed hot, then force colder prices onto cold maps.
    book.addOrder(mk(1, 100, 1, Side::Buy));
    book.addOrder(mk(2, 90, 1, Side::Buy));
    book.addOrder(mk(3, 90, 2, Side::Buy));

    book.addOrder(mk(4, 110, 1, Side::Sell));
    book.addOrder(mk(5, 120, 1, Side::Sell));
    book.addOrder(mk(6, 120, 2, Side::Sell));

    // Cancel cold orders to execute cold-side cancellation code paths.
    book.cancelOrder(3);
    book.cancelOrder(6);
    book.cancelOrder(1);

    // Non-zero modify path should keep order alive and affect match quantity.
    book.modifyOrder(2, 7);
    book.addOrder(mk(7, 90, 20, Side::Sell));

    auto trades = book.match();
    ASSERT_FALSE(trades.empty());
    EXPECT_EQ(trades.front().buyOrderId, 2u);
    EXPECT_EQ(trades.front().quantity, 7u);
}

TEST(HybridOrderBookTest, BestAskFallsBackToColdWhenHotEmpty)
{
    HybridOrderBook book(1);

    book.addOrder(mk(11, 101, 1, Side::Sell));
    book.addOrder(mk(12, 120, 1, Side::Sell));

    book.cancelOrder(11);

    EXPECT_EQ(book.getBestAsk(), 120u);
}

TEST(HybridOrderBookTest, ColdCancelErasesLastLevelForBothSides)
{
    HybridOrderBook book(1);

    book.addOrder(mk(1, 100, 1, Side::Buy));
    book.addOrder(mk(2, 90, 1, Side::Buy)); // cold bid
    book.cancelOrder(2);                    // erase cold bid level

    book.addOrder(mk(3, 110, 1, Side::Sell));
    book.addOrder(mk(4, 120, 1, Side::Sell)); // cold ask
    book.cancelOrder(4);                      // erase cold ask level

    EXPECT_EQ(book.getOrderCount(), 2u);
    EXPECT_EQ(book.getBestBid(), 100u);
    EXPECT_EQ(book.getBestAsk(), 110u);
}

TEST(HybridOrderBookTest, SamePriceHotLevelsAppendWithoutNewLevel)
{
    HybridOrderBook book(3);

    book.addOrder(mk(21, 101, 2, Side::Buy));
    book.addOrder(mk(22, 101, 3, Side::Buy));

    book.addOrder(mk(31, 101, 2, Side::Sell));
    book.addOrder(mk(32, 101, 3, Side::Sell));

    auto trades = book.match();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].quantity, 2u);
    EXPECT_EQ(trades[1].quantity, 3u);
}

TEST(HybridOrderBookTest, ZeroHotLevelsExercisesDemoteAndPromoteGuards)
{
    HybridOrderBook book(0);

    // Buy side: first insert triggers demote-on-empty guard, second goes cold.
    book.addOrder(mk(41, 100, 1, Side::Buy));
    book.addOrder(mk(42, 90, 1, Side::Buy));
    book.cancelOrder(41); // Keep only cold bids.

    // Sell side: same pattern.
    book.addOrder(mk(51, 100, 1, Side::Sell));
    book.addOrder(mk(52, 110, 1, Side::Sell));
    book.cancelOrder(51); // Keep only cold asks.

    // match() now promotes from cold on both sides with maxHotLevels_=0.
    auto trades = book.match();
    EXPECT_TRUE(trades.empty());
}

TEST(HybridOrderBookTest, HotCancelCanLeaveLevelNonEmpty)
{
    HybridOrderBook book(4);

    // Keep both prices hot and place multiple orders at one hot price level.
    book.addOrder(mk(61, 150, 1, Side::Buy));
    book.addOrder(mk(62, 150, 1, Side::Buy));
    book.addOrder(mk(63, 149, 1, Side::Buy));

    book.cancelOrder(61);
    EXPECT_EQ(book.getBestBid(), 150u);

    book.addOrder(mk(71, 160, 1, Side::Sell));
    book.addOrder(mk(72, 160, 1, Side::Sell));
    book.addOrder(mk(73, 161, 1, Side::Sell));

    book.cancelOrder(71);
    EXPECT_EQ(book.getBestAsk(), 160u);
}

} // namespace
} // namespace hft
