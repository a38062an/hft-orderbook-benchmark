#pragma once

#include "Types.hpp"

namespace hft 
{

struct Trade 
{
    OrderId buyOrderId;
    OrderId sellOrderId;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

} // namespace hft
