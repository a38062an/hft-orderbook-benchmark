#pragma once

#include "../core/IOrderBook.hpp"
#include <vector>
#include <unordered_map>
#include <list>

namespace hft
{

    class VectorOrderBook : public IOrderBook
    {
    public:
        void addOrder(const Order &order) override;
        void cancelOrder(OrderId orderId) override;
        void modifyOrder(OrderId orderId, Quantity newQuantity) override;
        std::vector<Trade> match() override;
        std::size_t getOrderCount() const override;

        Price getBestBid() const override;
        Price getBestAsk() const override;

    private:
        using OrderList = std::list<Order>;
        using BidsVector = std::vector<std::pair<Price, OrderList>>;
        using AsksVector = std::vector<std::pair<Price, OrderList>>;

        BidsVector bids_;
        AsksVector asks_;

        // For O(1) access to orders by ID
        struct OrderLocation
        {
            bool isBuy;
            std::size_t vectorIndex;
            OrderList::iterator iterator;
        };

        using OrderLookupMap = std::unordered_map<OrderId, OrderLocation>; // Fast order lookup by ID

        OrderLookupMap orderLookup_;
    };

} // namespace hft