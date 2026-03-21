#include <gtest/gtest.h>

#include "orderbooks/map_order_book.hpp"

namespace hft
{
namespace
{

Order mk(OrderId id, Price price, Quantity qty, Side side)
{
    return Order{id, price, qty, side, OrderType::Limit, 0, 0, 0};
}

TEST(MapOrderBookBranchTest, CancelCanLeavePriceLevelNonEmpty)
{
    MapOrderBook book;

    book.addOrder(mk(1, 120, 1, Side::Buy));
    book.addOrder(mk(2, 120, 1, Side::Buy));
    book.cancelOrder(1);
    EXPECT_EQ(book.getBestBid(), 120u);

    book.addOrder(mk(3, 130, 1, Side::Sell));
    book.addOrder(mk(4, 130, 1, Side::Sell));
    book.cancelOrder(3);
    EXPECT_EQ(book.getBestAsk(), 130u);
}

} // namespace
} // namespace hft
