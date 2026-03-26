#pragma once

#include "types.hpp"

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
    uint64_t receiveTimestamp; // When the order entered the exchange gateway
    uint64_t sendTimestamp;    // When the order was sent by the client (E2E start)

    // Padding to ensure cache line alignment or specific size if needed
    // For now, keep it simple and packed
};

} // namespace hft
