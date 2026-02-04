#pragma once

#include "Order.hpp"
#include "Trade.hpp"
#include <vector>
#include <limits>

namespace hft
{

    class IOrderBook
    {
    public:
        virtual ~IOrderBook() = default;

        virtual void addOrder(const Order &order) = 0;
        virtual void cancelOrder(OrderId orderId) = 0;
        virtual void modifyOrder(OrderId orderId, Quantity newQuantity) = 0; // Simplified modify

        // For benchmarking, we might want to return trades or just process them internally
        // Returning a vector might be slow, but good for correctness checking
        // In a real HFT system, we'd use a callback or a ring buffer
        virtual std::vector<Trade> match() = 0;

        virtual std::size_t getOrderCount() const = 0;

        // Get best bid/ask prices for market data, GUI, and matching logic
        // Returns 0 if no bids, std::numeric_limits<Price>::max() if no asks
        virtual Price getBestBid() const = 0;
        virtual Price getBestAsk() const = 0;
    };

} // namespace hft
