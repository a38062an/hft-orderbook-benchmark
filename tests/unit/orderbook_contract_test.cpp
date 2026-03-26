#include <gtest/gtest.h>

#include "core/order_book_factory.hpp"

namespace hft
{
namespace
{

Order makeOrder(OrderId id, Price price, Quantity qty, Side side)
{
    return Order{id, price, qty, side, OrderType::Limit, 0, 0, 0};
}

class OrderBookContractTest : public ::testing::TestWithParam<std::string>
{
  protected:
    void SetUp() override
    {
        book = OrderBookFactory::create(GetParam());
    }

    std::unique_ptr<IOrderBook> book;
};

INSTANTIATE_TEST_SUITE_P(AllBooks, OrderBookContractTest, ::testing::ValuesIn(OrderBookFactory::getSupportedTypes()));

// -----------------------------------------------------------------------------
// Happy Path Cases
// -----------------------------------------------------------------------------

TEST_P(OrderBookContractTest, HappyPath_AddAndBestPrices)
{
    book->addOrder(makeOrder(1, 120, 10, Side::Buy));
    book->addOrder(makeOrder(2, 130, 5, Side::Buy));
    book->addOrder(makeOrder(3, 140, 7, Side::Sell));
    book->addOrder(makeOrder(4, 150, 8, Side::Sell));

    EXPECT_EQ(book->getBestBid(), 130);
    EXPECT_EQ(book->getBestAsk(), 140);
    EXPECT_EQ(book->getOrderCount(), 4u);
}

TEST_P(OrderBookContractTest, HappyPath_CancelRemovesOrder)
{
    book->addOrder(makeOrder(10, 120, 10, Side::Buy));
    EXPECT_EQ(book->getOrderCount(), 1u);

    book->cancelOrder(10);

    EXPECT_EQ(book->getOrderCount(), 0u);
    EXPECT_EQ(book->getBestBid(), 0u);
}

TEST_P(OrderBookContractTest, HappyPath_CancelRemovesSellOrder)
{
    book->addOrder(makeOrder(11, 150, 10, Side::Sell));
    EXPECT_EQ(book->getOrderCount(), 1u);

    book->cancelOrder(11);

    EXPECT_EQ(book->getOrderCount(), 0u);
    EXPECT_EQ(book->getBestAsk(), std::numeric_limits<Price>::max());
}

TEST_P(OrderBookContractTest, HappyPath_MatchProducesExpectedTrade)
{
    book->addOrder(makeOrder(101, 130, 10, Side::Buy));
    book->addOrder(makeOrder(102, 130, 4, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buyOrderId, 101u);
    EXPECT_EQ(trades[0].sellOrderId, 102u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 4u);
    EXPECT_EQ(book->getOrderCount(), 1u);
}

TEST_P(OrderBookContractTest, HappyPath_CrossedTradeExecutesAtPassiveRestingPrice)
{
    // Resting ask is passive; incoming crossed buy should trade at ask price (130).
    book->addOrder(makeOrder(1201, 130, 5, Side::Sell));
    book->addOrder(makeOrder(1202, 140, 5, Side::Buy));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buyOrderId, 1202u);
    EXPECT_EQ(trades[0].sellOrderId, 1201u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_EQ(book->getOrderCount(), 0u);
}

TEST_P(OrderBookContractTest, HappyPath_ModifyNonZeroAffectsMatchedQuantity)
{
    book->addOrder(makeOrder(301, 130, 5, Side::Buy));
    book->modifyOrder(301, 11);
    book->addOrder(makeOrder(302, 130, 20, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buyOrderId, 301u);
    EXPECT_EQ(trades[0].sellOrderId, 302u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 11u);
    EXPECT_EQ(book->getOrderCount(), 1u);
}

// -----------------------------------------------------------------------------
// Edge Cases
// -----------------------------------------------------------------------------

TEST_P(OrderBookContractTest, EdgeCase_EmptyBookBestPrices)
{
    EXPECT_EQ(book->getBestBid(), 0u);
    EXPECT_EQ(book->getBestAsk(), std::numeric_limits<Price>::max());
}

TEST_P(OrderBookContractTest, EdgeCase_ModifyToZeroCancels)
{
    book->addOrder(makeOrder(21, 125, 9, Side::Buy));
    EXPECT_EQ(book->getOrderCount(), 1u);

    book->modifyOrder(21, 0);

    EXPECT_EQ(book->getOrderCount(), 0u);
}

TEST_P(OrderBookContractTest, EdgeCase_FifoAtSinglePrice)
{
    book->addOrder(makeOrder(201, 130, 3, Side::Buy));
    book->addOrder(makeOrder(202, 130, 3, Side::Buy));
    book->addOrder(makeOrder(203, 130, 3, Side::Sell));
    book->addOrder(makeOrder(204, 130, 3, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].buyOrderId, 201u);
    EXPECT_EQ(trades[0].sellOrderId, 203u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 3u);
    EXPECT_EQ(trades[1].buyOrderId, 202u);
    EXPECT_EQ(trades[1].sellOrderId, 204u);
    EXPECT_EQ(trades[1].price, 130u);
    EXPECT_EQ(trades[1].quantity, 3u);
    EXPECT_EQ(book->getOrderCount(), 0u);
}

TEST_P(OrderBookContractTest, EdgeCase_NonOverlappingPricesDoNotTrade)
{
    book->addOrder(makeOrder(401, 100, 2, Side::Buy));
    book->addOrder(makeOrder(402, 110, 2, Side::Sell));

    auto trades = book->match();

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book->getOrderCount(), 2u);
}

TEST_P(OrderBookContractTest, EdgeCase_PartialFillSellSideSurvives)
{
    // Ask side survives with remainder after first match.
    book->addOrder(makeOrder(501, 130, 4, Side::Buy));
    book->addOrder(makeOrder(502, 130, 10, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buyOrderId, 501u);
    EXPECT_EQ(trades[0].sellOrderId, 502u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 4u);
    EXPECT_EQ(book->getOrderCount(), 1u);
    EXPECT_EQ(book->getBestAsk(), 130u);
}

TEST_P(OrderBookContractTest, EdgeCase_PartialFillBuySideSurvives)
{
    // Bid side survives with remainder after first match.
    book->addOrder(makeOrder(511, 130, 10, Side::Buy));
    book->addOrder(makeOrder(512, 130, 4, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buyOrderId, 511u);
    EXPECT_EQ(trades[0].sellOrderId, 512u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 4u);
    EXPECT_EQ(book->getOrderCount(), 1u);
    EXPECT_EQ(book->getBestBid(), 130u);
}

TEST_P(OrderBookContractTest, EdgeCase_AddLowerBidDoesNotChangeBestBid)
{
    book->addOrder(makeOrder(601, 130, 2, Side::Buy));
    EXPECT_EQ(book->getBestBid(), 130u);

    book->addOrder(makeOrder(602, 120, 2, Side::Buy));
    EXPECT_EQ(book->getBestBid(), 130u);
    EXPECT_EQ(book->getOrderCount(), 2u);
}

TEST_P(OrderBookContractTest, EdgeCase_AddHigherAskDoesNotChangeBestAsk)
{
    book->addOrder(makeOrder(611, 130, 2, Side::Sell));
    EXPECT_EQ(book->getBestAsk(), 130u);

    book->addOrder(makeOrder(612, 140, 2, Side::Sell));
    EXPECT_EQ(book->getBestAsk(), 130u);
    EXPECT_EQ(book->getOrderCount(), 2u);
}

TEST_P(OrderBookContractTest, EdgeCase_CancelBestBidFallsBackToNextLevel)
{
    book->addOrder(makeOrder(621, 130, 2, Side::Buy));
    book->addOrder(makeOrder(622, 120, 2, Side::Buy));
    EXPECT_EQ(book->getBestBid(), 130u);

    book->cancelOrder(621);

    EXPECT_EQ(book->getBestBid(), 120u);
    EXPECT_EQ(book->getOrderCount(), 1u);
}

TEST_P(OrderBookContractTest, EdgeCase_CancelBestAskFallsBackToNextLevel)
{
    book->addOrder(makeOrder(631, 130, 2, Side::Sell));
    book->addOrder(makeOrder(632, 140, 2, Side::Sell));
    EXPECT_EQ(book->getBestAsk(), 130u);

    book->cancelOrder(631);

    EXPECT_EQ(book->getBestAsk(), 140u);
    EXPECT_EQ(book->getOrderCount(), 1u);
}

TEST_P(OrderBookContractTest, EdgeCase_OneBidMatchesMultipleAsks)
{
    book->addOrder(makeOrder(701, 130, 10, Side::Buy));
    book->addOrder(makeOrder(702, 130, 4, Side::Sell));
    book->addOrder(makeOrder(703, 130, 6, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].buyOrderId, 701u);
    EXPECT_EQ(trades[0].sellOrderId, 702u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 4u);
    EXPECT_EQ(trades[1].buyOrderId, 701u);
    EXPECT_EQ(trades[1].sellOrderId, 703u);
    EXPECT_EQ(trades[1].price, 130u);
    EXPECT_EQ(trades[1].quantity, 6u);
    EXPECT_EQ(book->getOrderCount(), 0u);
}

TEST_P(OrderBookContractTest, EdgeCase_OneAskMatchesMultipleBids)
{
    book->addOrder(makeOrder(711, 130, 4, Side::Buy));
    book->addOrder(makeOrder(712, 130, 6, Side::Buy));
    book->addOrder(makeOrder(713, 130, 10, Side::Sell));

    auto trades = book->match();

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].buyOrderId, 711u);
    EXPECT_EQ(trades[0].sellOrderId, 713u);
    EXPECT_EQ(trades[0].price, 130u);
    EXPECT_EQ(trades[0].quantity, 4u);
    EXPECT_EQ(trades[1].buyOrderId, 712u);
    EXPECT_EQ(trades[1].sellOrderId, 713u);
    EXPECT_EQ(trades[1].price, 130u);
    EXPECT_EQ(trades[1].quantity, 6u);
    EXPECT_EQ(book->getOrderCount(), 0u);
}

// -----------------------------------------------------------------------------
// Error Cases
// -----------------------------------------------------------------------------

TEST_P(OrderBookContractTest, ErrorCase_UnknownOperationsAreNoop)
{
    EXPECT_EQ(book->getOrderCount(), 0u);

    book->cancelOrder(9999);
    book->modifyOrder(9999, 42);

    EXPECT_EQ(book->getOrderCount(), 0u);
}

TEST_P(OrderBookContractTest, ErrorCase_UnknownOperationsDoNotMutateExistingBook)
{
    book->addOrder(makeOrder(801, 130, 2, Side::Buy));

    book->cancelOrder(9999);
    book->modifyOrder(9999, 42);

    EXPECT_EQ(book->getOrderCount(), 1u);
    EXPECT_EQ(book->getBestBid(), 130u);
}

TEST_P(OrderBookContractTest, ErrorCase_CancelAlreadyCancelledOrderIsNoop)
{
    book->addOrder(makeOrder(811, 130, 2, Side::Buy));
    EXPECT_EQ(book->getOrderCount(), 1u);

    book->cancelOrder(811);
    book->cancelOrder(811);

    EXPECT_EQ(book->getOrderCount(), 0u);
    EXPECT_EQ(book->getBestBid(), 0u);
}

TEST_P(OrderBookContractTest, ErrorCase_DuplicateOrderIdIsRejected)
{
    book->addOrder(makeOrder(9001, 130, 2, Side::Buy));
    book->addOrder(makeOrder(9001, 140, 7, Side::Sell));

    EXPECT_EQ(book->getOrderCount(), 1u);
    EXPECT_EQ(book->getBestBid(), 130u);
    EXPECT_EQ(book->getBestAsk(), std::numeric_limits<Price>::max());
}

} // namespace
} // namespace hft
