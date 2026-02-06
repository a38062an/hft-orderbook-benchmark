#pragma once

#include "../core/IOrderBook.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <list>

namespace hft
{

    class HybridOrderBook : public IOrderBook
    {
    public:
        HybridOrderBook(std::size_t maxHotLevels = 20);

        void addOrder(const Order &order) override;
        void cancelOrder(OrderId orderId) override;
        void modifyOrder(OrderId orderId, Quantity newQuantity) override;
        std::vector<Trade> match() override;
        std::size_t getOrderCount() const override;

        Price getBestBid() const override;
        Price getBestAsk() const override;

    private:
        using OrderList = std::list<Order>;
        using HotBidsVector = std::vector<std::pair<Price, OrderList>>;
        using HotAsksVector = std::vector<std::pair<Price, OrderList>>;
        using ColdBidsMap = std::map<Price, OrderList, std::greater<Price>>;
        using ColdAsksMap = std::map<Price, OrderList, std::less<Price>>;

        HotBidsVector hotBids_;
        HotAsksVector hotAsks_;
        ColdBidsMap coldBids_;
        ColdAsksMap coldAsks_;

        std::size_t maxHotLevels_;

        // Track where each order lives
        struct OrderLocation
        {
            bool isBuy;
            bool isHot; // true = in vector, false = in map
            union
            {
                std::size_t vectorIndex;
                Price mapPrice;
            };
            OrderList::iterator iterator;
        };

        using OrderLookupMap = std::unordered_map<OrderId, OrderLocation>;

        OrderLookupMap orderLookup_;

        // Helper methods
        bool isCloseToSpread(Price price, bool isBuy) const;
        void promoteToHot(Price price, bool isBuy);
        void demoteFromHot(bool isBuy);
        void addToHot(const Order &order, bool isBuy);
        void addToCold(const Order &order, bool isBuy);
    };

} // namespace hft
