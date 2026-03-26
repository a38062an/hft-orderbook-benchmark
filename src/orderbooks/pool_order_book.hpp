#pragma once

#include "../core/i_order_book.hpp"
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

namespace hft
{
struct OrderNode
{
    Order order;
    Index next = NULL_IDX;     // Intrusive linked list next pointer
    Index prev = NULL_IDX;     // Intrusive linked list prev pointer
    Index nextFree = NULL_IDX; // For free list management
};

struct PoolLimitLevel
{
    Price price;
    Index head = NULL_IDX;
    Index tail = NULL_IDX;
};

class PoolOrderBook : public IOrderBook
{
  public:
    explicit PoolOrderBook(Index maxOrders = 1000000);

    void addOrder(const Order &order) override;
    void cancelOrder(OrderId orderId) override;
    void modifyOrder(OrderId orderId, Quantity newQuantity) override;
    std::vector<Trade> match() override;

    Index getOrderCount() const override;
    Price getBestBid() const override;
    Price getBestAsk() const override;

  private:
    using BidsMap = std::map<Price, PoolLimitLevel, std::greater<Price>>; // Highest price first
    using AsksMap = std::map<Price, PoolLimitLevel, std::less<Price>>;    // Lowest price first
    using OrderLookupMap = std::unordered_map<OrderId, Index>;            // Fast lookup by ID

    std::vector<OrderNode> orders_;
    Index freeHead_ = NULL_IDX;

    OrderLookupMap orderLookup_;

    BidsMap bids_;
    AsksMap asks_;

    Index allocateSlot();
    void freeSlot(Index idx);

    template <typename MapType> void appendToLevel(MapType &ladder, Index idx, Price price);

    template <typename MapType> void removeFromLevel(MapType &ladder, Index idx);
};

} // namespace hft
