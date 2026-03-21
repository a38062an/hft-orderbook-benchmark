#include <gtest/gtest.h>

#include "orderbooks/vector_order_book.hpp"

namespace hft
{
namespace
{

Order mk(OrderId id, Price price, Quantity qty, Side side)
{
    return Order{id, price, qty, side, OrderType::Limit, 0, 0, 0};
}

TEST(VectorOrderBookBranchTest, BuyInsertBetweenExistingLevels)
{
    VectorOrderBook book;

    book.addOrder(mk(1, 110, 1, Side::Buy));
    book.addOrder(mk(2, 100, 1, Side::Buy));

    // Insert between two levels: lower_bound returns non-end iterator whose price is not equal.
    book.addOrder(mk(3, 105, 1, Side::Buy));

    EXPECT_EQ(book.getBestBid(), 110u);
    EXPECT_EQ(book.getOrderCount(), 3u);
}

TEST(VectorOrderBookBranchTest, SellInsertBetweenExistingLevels)
{
    VectorOrderBook book;

    book.addOrder(mk(11, 100, 1, Side::Sell));
    book.addOrder(mk(12, 110, 1, Side::Sell));

    // Insert between two levels: lower_bound returns non-end iterator whose price is not equal.
    book.addOrder(mk(13, 105, 1, Side::Sell));

    EXPECT_EQ(book.getBestAsk(), 100u);
    EXPECT_EQ(book.getOrderCount(), 3u);
}

TEST(VectorOrderBookBranchTest, CancelCanLeavePriceLevelNonEmpty)
{
    VectorOrderBook book;

    book.addOrder(mk(21, 120, 1, Side::Buy));
    book.addOrder(mk(22, 120, 1, Side::Buy));
    book.cancelOrder(21);
    EXPECT_EQ(book.getBestBid(), 120u);

    book.addOrder(mk(31, 130, 1, Side::Sell));
    book.addOrder(mk(32, 130, 1, Side::Sell));
    book.cancelOrder(31);
    EXPECT_EQ(book.getBestAsk(), 130u);
}

} // namespace
} // namespace hft
