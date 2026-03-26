#pragma once

#include "types.hpp"

namespace hft
{

struct Trade
{
    OrderId buyOrderId;
    OrderId sellOrderId;
    Price price;
    Quantity quantity;
};

} // namespace hft
