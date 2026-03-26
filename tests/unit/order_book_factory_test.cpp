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

TEST(OrderBookFactoryTest, CanCreateAllAdvertisedTypes)
{
    const auto types = OrderBookFactory::getSupportedTypes();
    ASSERT_FALSE(types.empty());
    for (const auto &type : types)
    {
        EXPECT_NO_THROW({ auto b = OrderBookFactory::create(type); }) << "Failed creating type: " << type;
    }
}

TEST(OrderBookFactoryTest, SupportedTypesAreUnique)
{
    const auto types = OrderBookFactory::getSupportedTypes();
    std::vector<std::string> sorted = types;
    std::sort(sorted.begin(), sorted.end());
    auto it = std::adjacent_find(sorted.begin(), sorted.end());
    EXPECT_EQ(it, sorted.end()) << "Duplicate factory type: " << *it;
}

} // namespace
} // namespace hft
