#pragma once

#include <cstdint>


namespace hft 
{

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;
using Timestamp = uint64_t;

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
