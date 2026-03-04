#pragma once

#include <cstdint>
#include <limits>


namespace hft 
{

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;
using Timestamp = uint64_t;
using Index = std::size_t;
constexpr Index NULL_IDX = std::numeric_limits<Index>::max();

enum class Side : uint8_t 
{
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t 
{
    Limit = 0,
    Market = 1
};

} // namespace hft
