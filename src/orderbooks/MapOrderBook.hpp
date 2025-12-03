#pragma once

#include "core/IOrderBook.hpp"
#include <map>
#include <unordered_map>
#include <list>

namespace hft 
{

class MapOrderBook : public IOrderBook 
{
public:
    void addOrder(const Order& order) override;
    void cancelOrder(OrderId orderId) override;
    void modifyOrder(OrderId orderId, Quantity newQuantity) override;
    std::vector<Trade> match() override;
    std::size_t getOrderCount() const override;

private:
    using OrderList = std::list<Order>;
    using BidsMap = std::map<Price, OrderList, std::greater<Price>>; // Highest price first
    using AsksMap = std::map<Price, OrderList, std::less<Price>>;    // Lowest price first
    
    BidsMap bids_;
    AsksMap asks_;
    
    // For O(1) access to orders by ID
    struct OrderLocation 
    {
        bool isBuy;
        Price price;
        OrderList::iterator iterator;
    };
    std::unordered_map<OrderId, OrderLocation> orderLookup_;
};

} // namespace hft
