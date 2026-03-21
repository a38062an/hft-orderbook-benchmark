#include <gtest/gtest.h>

#include "orderbooks/array_order_book.hpp"
#include "orderbooks/hybrid_order_book.hpp"
#include "orderbooks/map_order_book.hpp"
#include "orderbooks/vector_order_book.hpp"

namespace hft
{
namespace
{

template <typename BookFactory> void runThousandSingleTradeMatches(BookFactory makeBook)
{
    auto book = makeBook();

    for (OrderId i = 1; i <= 1000; ++i)
    {
        Order buy{i * 2, 130, 1, Side::Buy, OrderType::Limit, 0, 0, 0};
        Order sell{i * 2 + 1, 130, 1, Side::Sell, OrderType::Limit, 0, 0, 0};

        book.addOrder(buy);
        book.addOrder(sell);

        auto trades = book.match();
        ASSERT_EQ(trades.size(), 1u);
    }
}

TEST(OrderBookBranchStressTest, HitsMapOrderBookThousandthMatchBranch)
{
    runThousandSingleTradeMatches([] { return MapOrderBook{}; });
}

TEST(OrderBookBranchStressTest, HitsVectorOrderBookThousandthMatchBranch)
{
    runThousandSingleTradeMatches([] { return VectorOrderBook{}; });
}

TEST(OrderBookBranchStressTest, HitsArrayOrderBookThousandthMatchBranch)
{
    runThousandSingleTradeMatches([] { return ArrayOrderBook(100, 200, 1); });
}

TEST(OrderBookBranchStressTest, HitsHybridOrderBookThousandthMatchBranch)
{
    runThousandSingleTradeMatches([] { return HybridOrderBook{}; });
}

} // namespace
} // namespace hft
