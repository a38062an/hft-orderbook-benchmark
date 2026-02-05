#include "ArrayOrderBook.hpp"
#include <iostream>
#include <stdexcept>

namespace hft
{
    ArrayOrderBook::ArrayOrderBook(Price minPrice, Price maxPrice, Price tickSize)
        : minPrice_(minPrice),
          maxPrice_(maxPrice),
          tickSize_(tickSize),
          numLevels_((maxPrice - minPrice) / tickSize + 1),
          bidLevels_(numLevels_),
          askLevels_(numLevels_),
          activeBidLevels_(numLevels_, false),
          activeAskLevels_(numLevels_, false),
          cachedBestBid_(0),
          cachedBestAsk_(std::numeric_limits<Price>::max())
    {
        // Validate configuration
        if (minPrice >= maxPrice)
        {
            throw std::invalid_argument("minPrice must be less than maxPrice");
        }
        if (tickSize == 0)
        {
            throw std::invalid_argument("tickSize must be greater than 0");
        }
        if ((maxPrice - minPrice) % tickSize != 0)
        {
            throw std::invalid_argument("Price range must be evenly divisible by tickSize");
        }
    }

    void ArrayOrderBook::addOrder(const Order &order)
    {
        std::size_t indexToInsert = priceToIndex(order.price);

        if (order.side == Side::Buy)
        {
            // Insert order into order list at appropriate index
            auto &orderList = bidLevels_[indexToInsert];
            orderList.push_back(order);

            // Update the active BidLevels and cache
            activeBidLevels_[indexToInsert] = true;
            if (order.price > cachedBestBid_)
            {
                cachedBestBid_ = order.price;
            }

            // Store iterator in lookup
            orderLookup_[order.id] = {true, indexToInsert, --orderList.end()};
        }
        else
        {

            auto &orderList = askLevels_[indexToInsert];
            orderList.push_back(order);

            // Update the active BidLevels and cache
            activeAskLevels_[indexToInsert] = true;
            if (order.price > cachedBestAsk_)
            {
                cachedBestBid_ = order.price;
            }

            // Store iterator in lookup
            orderLookup_[order.id] = {true, indexToInsert, --orderList.end()};
        }
    }

    void ArrayOrderBook::cancelOrder(OrderId orderId)
    {
        auto orderIterator = orderLookup_.find(orderId);
        if (orderIterator == orderLookup_.end())
        {
            return; // Order not found
        }

        const auto &orderLocation = orderIterator->second;
        std::size_t indexToCancel = orderLocation.arrayIndex;

        if (orderLocation.isBuy)
        {
            bidLevels_[indexToCancel].erase(orderLocation.iterator);

            // If emptied price level must update activeBidLevels and potentially update Cache
            if (bidLevels_[indexToCancel].empty())
            {
                activeBidLevels_[indexToCancel] = false;

                if (indexToPrice(indexToCancel) == cachedBestBid_)
                {
                    updateBestBidCache();
                }
            }
        }
        else
        {
            askLevels_[indexToCancel].erase(orderLocation.iterator);

            // If emptied price level must update activeBidLevels and potentially update Cache
            if (askLevels_[indexToCancel].empty())
            {
                activeAskLevels_[indexToCancel] = false;

                if (indexToPrice(indexToCancel) == cachedBestAsk_)
                {
                    updateBestAskCache();
                }
            }
        }

        orderLookup_.erase(orderIterator);
    }

    void ArrayOrderBook::modifyOrder(OrderId orderId, Quantity newQuantity)
    {
        auto orderIterator = orderLookup_.find(orderId);
        if (orderIterator == orderLookup_.end())
        {
            return; // Couldn't find order (non existent)
        }

        // If modifying to zero quantity, cancel instead
        if (newQuantity == 0)
        {
            cancelOrder(orderId);
            return;
        }

        const auto &orderLocation = orderIterator->second;

        orderLocation.iterator->quantity = newQuantity;
    }

    std::vector<Trade> ArrayOrderBook::match()
    {
        std::vector<Trade> trades;
        static uint64_t matchCounter{0};

        while (cachedBestBid_ >= cachedBestAsk_)
        {
            std::size_t bidIndex = priceToIndex(cachedBestBid_);
            std::size_t askIndex = priceToIndex(cachedBestAsk_);

            auto &bidList = bidLevels_[bidIndex];
            auto &askList = askLevels_[askIndex];

            // Price time priority
            Order &bidOrder = bidList.front();
            Order &askOrder = askList.front();

            Quantity tradeQty = std::min(bidOrder.quantity, askOrder.quantity);

            // Create trade
            trades.push_back({bidOrder.id,
                              askOrder.id,
                              askOrder.price,
                              tradeQty});

            matchCounter++;
            if (matchCounter % 1000 == 0)
            {
                std::cout << "Match #" << matchCounter << ": "
                          << "Bid=" << bidOrder.id << " "
                          << "Ask=" << askOrder.id << " "
                          << "Price=" << askOrder.price << " "
                          << "Qty=" << tradeQty << std::endl;
            }

            // Update quantities
            bidOrder.quantity -= tradeQty;
            askOrder.quantity -= tradeQty;

            // Clean up empty price levels (must update caches and activeLevels on empty)
            if (bidOrder.quantity == 0)
            {
                orderLookup_.erase(bidOrder.id);
                bidList.pop_front();
                if (bidList.empty())
                {
                    activeBidLevels_[bidIndex] = false;
                    updateBestBidCache();
                }
            }

            if (askOrder.quantity == 0)
            {
                orderLookup_.erase(askOrder.id);
                askList.pop_front();
                if (askList.empty())
                {
                    activeAskLevels_[bidIndex] = false;
                    updateBestAskCache();
                }
            }
        }
        return trades;
    }

    // Helpers
    std::size_t ArrayOrderBook::priceToIndex(Price price) const
    {
        return (price - minPrice_) / tickSize_;
    }

    Price ArrayOrderBook::indexToPrice(std::size_t index) const
    {
        return minPrice_ + (index * tickSize_);
    }

    bool ArrayOrderBook::isValidPrice(Price price) const
    {
        // Check if price is in range
        if (price < minPrice_ || price > maxPrice_)
        {
            return false;
        }
        // Check if price is aligned to tick size
        if ((price - minPrice_) % tickSize_ != 0)
        {
            return false;
        }
        return true;
    }

    void ArrayOrderBook::updateBestBidCache()
    {
        // Scan from highest price to lowest (bids want highest price)
        for (int i = numLevels_ - 1; i >= 0; --i)
        {
            if (activeBidLevels_[i])
            {
                cachedBestBid_ = indexToPrice(i);
                return;
            }
        }
        // No bids found
        cachedBestBid_ = 0;
    }

    void ArrayOrderBook::updateBestAskCache()
    {
        // Scan from lowest price to highest (asks want lowest price)
        for (std::size_t i = 0; i < numLevels_; ++i)
        {
            if (activeAskLevels_[i])
            {
                cachedBestAsk_ = indexToPrice(i);
                return;
            }
        }
        // No asks found
        cachedBestAsk_ = std::numeric_limits<Price>::max();
    }

    // Public APIS for future GUI
    Price ArrayOrderBook::getBestBid() const
    {
        return cachedBestBid_;
    }

    Price ArrayOrderBook::getBestAsk() const
    {
        return cachedBestAsk_;
    }

    std::size_t ArrayOrderBook::getOrderCount() const
    {
        return orderLookup_.size();
    }

}
