#include <gtest/gtest.h>

#include "orderbooks/pool_order_book.hpp"

namespace hft
{
namespace
{

TEST(PoolOrderBookTest, ThrowsWhenPoolExhausted)
{
    PoolOrderBook book(2);

    Order a{1, 130, 5, Side::Buy, OrderType::Limit, 0, 0, 0};
    Order b{2, 131, 5, Side::Buy, OrderType::Limit, 0, 0, 0};
    Order c{3, 132, 5, Side::Buy, OrderType::Limit, 0, 0, 0};

    book.addOrder(a);
    book.addOrder(b);
    EXPECT_THROW(book.addOrder(c), std::runtime_error);
}

TEST(PoolOrderBookTest, CancelMiddleNodeAtPriceLevel)
{
    PoolOrderBook book(10);

    // Three orders at same bid level so canceling id=2 removes non-head node.
    book.addOrder(Order{1, 130, 1, Side::Buy, OrderType::Limit, 0, 0, 0});
    book.addOrder(Order{2, 130, 1, Side::Buy, OrderType::Limit, 0, 0, 0});
    book.addOrder(Order{3, 130, 1, Side::Buy, OrderType::Limit, 0, 0, 0});

    book.cancelOrder(2);

    EXPECT_EQ(book.getOrderCount(), 2u);
    EXPECT_EQ(book.getBestBid(), 130u);
}

TEST(PoolOrderBookTest, EmptyAskReturnsMaxSentinel)
{
    PoolOrderBook book(4);
    EXPECT_EQ(book.getBestAsk(), std::numeric_limits<Price>::max());
}

TEST(PoolOrderBookTest, CancelMiddleAskNodeAtPriceLevel)
{
    PoolOrderBook book(10);

    book.addOrder(Order{11, 140, 1, Side::Sell, OrderType::Limit, 0, 0, 0});
    book.addOrder(Order{12, 140, 1, Side::Sell, OrderType::Limit, 0, 0, 0});
    book.addOrder(Order{13, 140, 1, Side::Sell, OrderType::Limit, 0, 0, 0});

    book.cancelOrder(12);

    EXPECT_EQ(book.getOrderCount(), 2u);
    EXPECT_EQ(book.getBestAsk(), 140u);
}

} // namespace
} // namespace hft
