#pragma once

#include "../core/IOrderBook.hpp"
#include <array>
#include <bitset>
#include <list>
#include <unordered_map>
#include <limits>

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
        std::size_t getOrderCount() const override;

        Price getBestBid() const override;
        Price getBestAsk() const override;

        Price getMinPrice() const { return minPrice_; }
        Price getMaxPrice() const { return maxPrice_; }
        Price getTickSize() const { return tickSize_; }

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
        std::size_t numLevels_; // Calculated: (maxPrice - minPrice) / tickSize + 1

        BidLevelsArray bidLevels_;
        AskLevelsArray askLevels_;

        // Track which price levels have orders (activeBidLevels_[i].test(i) than bidLevels_[i].empty())
        ActiveLevelsBitset activeBidLevels_;
        ActiveLevelsBitset activeAskLevels_;

        // For O(1) order lookup by ID
        struct OrderLocation
        {
            bool isBuy;
            std::size_t arrayIndex;
            OrderList::iterator iterator;
        };

        using OrderLookupMap = std::unordered_map<OrderId, OrderLocation>;

        OrderLookupMap orderLookup_;

        // Cached best prices for O(1) access
        Price cachedBestBid_;
        Price cachedBestAsk_;

        // Helper methods
        std::size_t priceToIndex(Price price) const;
        Price indexToPrice(std::size_t index) const;
        bool isValidPrice(Price price) const;
        void updateBestBidCache();
        void updateBestAskCache();
    };

} // namespace hft
