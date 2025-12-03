#pragma once

#include "Types.hpp"

namespace hft 
{

struct Order 
{
    OrderId id;
    Price price;
    Quantity quantity;
    Side side;
    OrderType type;
    Timestamp timestamp;

    // Padding to ensure cache line alignment or specific size if needed
    // For now, keep it simple and packed
};

} // namespace hft
