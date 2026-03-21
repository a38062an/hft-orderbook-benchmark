#include <gtest/gtest.h>

#include "orderbooks/array_order_book.hpp"

namespace hft
{
namespace
{

Order makeOrder(OrderId id, Price price, Quantity qty, Side side)
{
    return Order{id, price, qty, side, OrderType::Limit, 0, 0, 0};
}

TEST(ArrayOrderBookTest, RejectsOutOfRangePrice)
{
    ArrayOrderBook book(100, 200, 1);

    book.addOrder(makeOrder(1, 120, 10, Side::Buy));
    book.addOrder(makeOrder(3, 99, 10, Side::Buy));
    book.addOrder(makeOrder(2, 250, 10, Side::Buy));

    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestBid(), 120u);
}

TEST(ArrayOrderBookTest, RejectsTickMisalignedPrice)
{
    ArrayOrderBook book(100, 200, 5);

    book.addOrder(makeOrder(1, 115, 10, Side::Buy));
    book.addOrder(makeOrder(2, 117, 10, Side::Buy));

    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestBid(), 115u);
}

TEST(ArrayOrderBookTest, ConstructorValidatesRangeAndTick)
{
    EXPECT_THROW(ArrayOrderBook(200, 100, 1), std::invalid_argument);
    EXPECT_THROW(ArrayOrderBook(100, 200, 0), std::invalid_argument);
    EXPECT_THROW(ArrayOrderBook(100, 201, 2), std::invalid_argument);
}

TEST(ArrayOrderBookTest, ExposesConfigurationGetters)
{
    ArrayOrderBook book(100, 200, 5);

    EXPECT_EQ(book.getMinPrice(), 100u);
    EXPECT_EQ(book.getMaxPrice(), 200u);
    EXPECT_EQ(book.getTickSize(), 5u);
}

TEST(ArrayOrderBookTest, BestBidAndAskCachesRescanWhenTopLevelsRemoved)
{
    ArrayOrderBook book(100, 200, 1);

    book.addOrder(makeOrder(1, 150, 2, Side::Buy));
    book.addOrder(makeOrder(2, 140, 2, Side::Buy));
    book.addOrder(makeOrder(3, 160, 2, Side::Sell));
    book.addOrder(makeOrder(4, 170, 2, Side::Sell));

    EXPECT_EQ(book.getBestBid(), 150u);
    EXPECT_EQ(book.getBestAsk(), 160u);

    book.cancelOrder(1);
    book.cancelOrder(3);

    EXPECT_EQ(book.getBestBid(), 140u);
    EXPECT_EQ(book.getBestAsk(), 170u);
}

TEST(ArrayOrderBookTest, ModifyNonZeroQuantityAffectsTradeSize)
{
    ArrayOrderBook book(100, 200, 1);

    book.addOrder(makeOrder(10, 130, 3, Side::Buy));
    book.modifyOrder(10, 9);
    book.addOrder(makeOrder(11, 130, 20, Side::Sell));

    auto trades = book.match();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades.front().quantity, 9u);
}

TEST(ArrayOrderBookTest, CancelCanLeaveLevelNonEmptyAndSkipBestCacheUpdate)
{
    ArrayOrderBook book(100, 200, 1);

    // Buy side: cancel one of two orders at the same best level (non-empty level path).
    book.addOrder(makeOrder(1, 150, 1, Side::Buy));
    book.addOrder(makeOrder(2, 150, 1, Side::Buy));
    book.cancelOrder(1);
    EXPECT_EQ(book.getBestBid(), 150u);

    // Buy side: empty a non-best level to take the "no best-cache update" path.
    book.addOrder(makeOrder(3, 140, 1, Side::Buy));
    book.cancelOrder(3);
    EXPECT_EQ(book.getBestBid(), 150u);

    // Sell side: cancel one of two orders at the same best level (non-empty level path).
    book.addOrder(makeOrder(4, 160, 1, Side::Sell));
    book.addOrder(makeOrder(5, 160, 1, Side::Sell));
    book.cancelOrder(4);
    EXPECT_EQ(book.getBestAsk(), 160u);

    // Sell side: empty a non-best level to take the "no best-cache update" path.
    book.addOrder(makeOrder(6, 170, 1, Side::Sell));
    book.cancelOrder(6);
    EXPECT_EQ(book.getBestAsk(), 160u);
}

} // namespace
} // namespace hft
