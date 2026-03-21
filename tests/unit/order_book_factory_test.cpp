#include <gtest/gtest.h>

#include <algorithm>

#include "core/order_book_factory.hpp"

namespace hft
{
namespace
{

TEST(OrderBookFactoryTest, SupportsExpectedTypes)
{
    const auto types = OrderBookFactory::getSupportedTypes();
    EXPECT_NE(std::find(types.begin(), types.end(), "map"), types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), "vector"), types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), "array"), types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), "hybrid"), types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), "pool"), types.end());
}

TEST(OrderBookFactoryTest, ThrowsForUnknownType)
{
    EXPECT_THROW(OrderBookFactory::create("unknown"), std::runtime_error);
}

} // namespace
} // namespace hft
