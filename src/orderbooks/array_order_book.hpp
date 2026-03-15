#pragma once

#include "../core/i_order_book.hpp"
#include <array>
#include <bitset>
#include <limits>
#include <list>
#include <unordered_map>

namespace hft
{

class ArrayOrderBook : public IOrderBook
{
  public:
    // Constructor with configurable price range and tick size
    ArrayOrderBook(Price minPrice, Price maxPrice, Price tickSize);

    void addOrder(const Order &order) override;
    void cancelOrder(OrderId orderId) override;
    void modifyOrder(OrderId orderId, Quantity newQuantity) override;
    std::vector<Trade> match() override;
    Index getOrderCount() const override;

    Price getBestBid() const override;
    Price getBestAsk() const override;

    Price getMinPrice() const
    {
        return minPrice_;
    }
    Price getMaxPrice() const
    {
        return maxPrice_;
    }
    Price getTickSize() const
    {
        return tickSize_;
    }

  private:
    // Type aliases
    using OrderList = std::list<Order>;
    using BidLevelsArray = std::vector<OrderList>; // Dynamic size based on price range
    using AskLevelsArray = std::vector<OrderList>; // Dynamic size based on price range
    using ActiveLevelsBitset = std::vector<bool>;  // Dynamic bitset (vector<bool> is specialized)

    // Price range configuration
    Price minPrice_;
    Price maxPrice_;
    Price tickSize_;
    Index numLevels_; // Calculated: (maxPrice - minPrice) / tickSize + 1

    BidLevelsArray bidLevels_;
    AskLevelsArray askLevels_;

    // Track which price levels have orders (activeBidLevels_[i].test(i) than bidLevels_[i].empty())
    ActiveLevelsBitset activeBidLevels_;
    ActiveLevelsBitset activeAskLevels_;

    // For O(1) order lookup by ID
    struct OrderLocation
    {
        bool isBuy;
        Index arrayIndex;
        OrderList::iterator iterator;
    };

    using OrderLookupMap = std::unordered_map<OrderId, OrderLocation>;

    OrderLookupMap orderLookup_;

    // Cached best prices for O(1) access
    Price cachedBestBid_;
    Price cachedBestAsk_;

    // Helper methods
    Index priceToIndex(Price price) const;
    Price indexToPrice(Index index) const;
    bool isValidPrice(Price price) const;
    void updateBestBidCache();
    void updateBestAskCache();
};

} // namespace hft
