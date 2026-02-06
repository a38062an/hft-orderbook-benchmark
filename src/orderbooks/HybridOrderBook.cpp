#include "HybridOrderBook.hpp"
#include <iostream>
#include <algorithm>

namespace hft
{

    HybridOrderBook::HybridOrderBook(std::size_t maxHotLevels)
        : maxHotLevels_(maxHotLevels)
    {
    }

    // Adds a new order to the book.
    // Decides hot vs cold based on proximity to spread (top N price levels).
    void HybridOrderBook::addOrder(const Order &order)
    {
        if (order.side == Side::Buy)
        {
            // Check if price exists in hot path
            auto hotIt = std::lower_bound(
                hotBids_.begin(), hotBids_.end(), order.price,
                [](const auto &priceLevel, Price price)
                { return priceLevel.first > price; });

            if (hotIt != hotBids_.end() && hotIt->first == order.price)
            {
                // Price level exists in hot, add there
                hotIt->second.push_back(order);
                orderLookup_[order.id] = {true, true, {static_cast<std::size_t>(hotIt - hotBids_.begin())}, --hotIt->second.end()};
            }
            else
            {
                // Check cold storage
                auto coldIt = coldBids_.find(order.price);
                if (coldIt != coldBids_.end())
                {
                    // Price exists in cold, add there (no eager promotion)
                    coldIt->second.push_back(order);
                    OrderLocation loc;
                    loc.isBuy = true;
                    loc.isHot = false;
                    loc.mapPrice = order.price;
                    loc.iterator = --coldIt->second.end();
                    orderLookup_[order.id] = loc;
                }
                else
                {
                    // New price level - decide hot or cold by proximity
                    if (isCloseToSpread(order.price, true))
                    {
                        // Price in top N levels, make room if needed
                        if (hotBids_.size() >= maxHotLevels_)
                        {
                            demoteFromHot(true);
                        }
                        addToHot(order, true);
                    }
                    else
                    {
                        // Price too far from spread, goes to cold
                        addToCold(order, true);
                    }
                }
            }
        }
        else
        {
            // Check if price exists in hot path
            auto hotIt = std::lower_bound(
                hotAsks_.begin(), hotAsks_.end(), order.price,
                [](const auto &priceLevel, Price price)
                { return priceLevel.first < price; });

            if (hotIt != hotAsks_.end() && hotIt->first == order.price)
            {
                // Price level exists in hot, add there
                hotIt->second.push_back(order);
                orderLookup_[order.id] = {false, true, {static_cast<std::size_t>(hotIt - hotAsks_.begin())}, --hotIt->second.end()};
            }
            else
            {
                // Check cold storage
                auto coldIt = coldAsks_.find(order.price);
                if (coldIt != coldAsks_.end())
                {
                    // Price exists in cold, add there (no eager promotion)
                    coldIt->second.push_back(order);
                    OrderLocation loc;
                    loc.isBuy = false;
                    loc.isHot = false;
                    loc.mapPrice = order.price;
                    loc.iterator = --coldIt->second.end();
                    orderLookup_[order.id] = loc;
                }
                else
                {
                    // New price level - decide hot or cold by proximity
                    if (isCloseToSpread(order.price, false))
                    {
                        // Price in top N levels, make room if needed
                        if (hotAsks_.size() >= maxHotLevels_)
                        {
                            demoteFromHot(false);
                        }
                        addToHot(order, false);
                    }
                    else
                    {
                        // Price too far from spread, goes to cold
                        addToCold(order, false);
                    }
                }
            }
        }
    }

    // Cancels an existing order by ID.
    // Handles both hot and cold storage locations.
    void HybridOrderBook::cancelOrder(OrderId orderId)
    {
        auto orderIt = orderLookup_.find(orderId);
        if (orderIt == orderLookup_.end())
        {
            return; // Order not found
        }

        const auto &loc = orderIt->second;

        if (loc.isHot)
        {
            // Order in hot path
            if (loc.isBuy)
            {
                auto &orderList = hotBids_[loc.vectorIndex].second;
                orderList.erase(loc.iterator);

                // Clean up empty price levels
                if (orderList.empty())
                {
                    hotBids_.erase(hotBids_.begin() + loc.vectorIndex);

                    // Update indices for remaining orders
                    for (auto &pair : orderLookup_)
                    {
                        if (pair.second.isHot && pair.second.isBuy && pair.second.vectorIndex > loc.vectorIndex)
                        {
                            pair.second.vectorIndex--;
                        }
                    }
                }
            }
            else
            {
                auto &orderList = hotAsks_[loc.vectorIndex].second;
                orderList.erase(loc.iterator);

                if (orderList.empty())
                {
                    hotAsks_.erase(hotAsks_.begin() + loc.vectorIndex);

                    for (auto &pair : orderLookup_)
                    {
                        if (pair.second.isHot && !pair.second.isBuy && pair.second.vectorIndex > loc.vectorIndex)
                        {
                            pair.second.vectorIndex--;
                        }
                    }
                }
            }
        }
        else
        {
            // Order in cold storage
            if (loc.isBuy)
            {
                auto mapIt = coldBids_.find(loc.mapPrice);
                if (mapIt != coldBids_.end())
                {
                    mapIt->second.erase(loc.iterator);
                    if (mapIt->second.empty())
                    {
                        coldBids_.erase(mapIt);
                    }
                }
            }
            else
            {
                auto mapIt = coldAsks_.find(loc.mapPrice);
                if (mapIt != coldAsks_.end())
                {
                    mapIt->second.erase(loc.iterator);
                    if (mapIt->second.empty())
                    {
                        coldAsks_.erase(mapIt);
                    }
                }
            }
        }

        orderLookup_.erase(orderIt);
    }

    // Modifies the quantity of an existing order.
    void HybridOrderBook::modifyOrder(OrderId orderId, Quantity newQuantity)
    {
        auto orderIt = orderLookup_.find(orderId);
        if (orderIt == orderLookup_.end())
        {
            return;
        }

        // If modifying to zero quantity, cancel instead
        if (newQuantity == 0)
        {
            cancelOrder(orderId);
            return;
        }

        // In a real book, increasing size might lose priority.
        // For simplicity here, we just update the quantity in place.
        orderIt->second.iterator->quantity = newQuantity;
    }

    // Matches buy and sell orders based on Price-Time priority.
    // Lazy promotion: only promotes from cold when needed for matching.
    std::vector<Trade> HybridOrderBook::match()
    {
        std::vector<Trade> trades;
        static uint64_t matchCounter{0};

        while (true)
        {
            // Get best bid (check hot first, then cold)
            Price bestBidPrice = 0;
            bool bidInHot = !hotBids_.empty();
            if (bidInHot)
            {
                bestBidPrice = hotBids_.front().first;
            }
            else if (!coldBids_.empty())
            {
                bestBidPrice = coldBids_.begin()->first;
                // Promote from cold to hot
                promoteToHot(bestBidPrice, true);
                bidInHot = true;
            }
            else
            {
                break; // No bids
            }

            // Get best ask (check hot first, then cold)
            Price bestAskPrice = std::numeric_limits<Price>::max();
            bool askInHot = !hotAsks_.empty();
            if (askInHot)
            {
                bestAskPrice = hotAsks_.front().first;
            }
            else if (!coldAsks_.empty())
            {
                bestAskPrice = coldAsks_.begin()->first;
                // Promote from cold to hot
                promoteToHot(bestAskPrice, false);
                askInHot = true;
            }
            else
            {
                break; // No asks
            }

            // Check for price overlap
            if (bestBidPrice < bestAskPrice)
            {
                break; // No overlap, spread is open
            }

            // Match orders at this price level
            auto &bidOrderList = hotBids_.front().second;
            auto &askOrderList = hotAsks_.front().second;

            while (!bidOrderList.empty() && !askOrderList.empty())
            {
                auto &bidOrder = bidOrderList.front();
                auto &askOrder = askOrderList.front();

                Quantity tradeQty = std::min(bidOrder.quantity, askOrder.quantity);

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
                              << "Price=" << bestAskPrice << " "
                              << "Qty=" << tradeQty << std::endl;
                }

                bidOrder.quantity -= tradeQty;
                askOrder.quantity -= tradeQty;

                // Remove filled orders
                if (bidOrder.quantity == 0)
                {
                    orderLookup_.erase(bidOrder.id);
                    bidOrderList.pop_front();
                }
                if (askOrder.quantity == 0)
                {
                    orderLookup_.erase(askOrder.id);
                    askOrderList.pop_front();
                }
            }

            // Clean up empty price levels
            if (bidOrderList.empty())
            {
                hotBids_.erase(hotBids_.begin());

                for (auto &pair : orderLookup_)
                {
                    if (pair.second.isHot && pair.second.isBuy && pair.second.vectorIndex > 0)
                    {
                        pair.second.vectorIndex--;
                    }
                }
            }

            if (askOrderList.empty())
            {
                hotAsks_.erase(hotAsks_.begin());

                for (auto &pair : orderLookup_)
                {
                    if (pair.second.isHot && !pair.second.isBuy && pair.second.vectorIndex > 0)
                    {
                        pair.second.vectorIndex--;
                    }
                }
            }
        }

        return trades;
    }

    // Checks if a price is close enough to spread to be in hot storage.
    // For bids: checks if price is in top N by rank (higher is better).
    // For asks: checks if price is in top N by rank (lower is better).
    bool HybridOrderBook::isCloseToSpread(Price price, bool isBuy) const
    {
        if (isBuy)
        {
            // For bids, check if in top maxHotLevels_ by price
            if (hotBids_.empty())
            {
                return true; // No hot levels yet, always add to hot
            }
            if (hotBids_.size() < maxHotLevels_)
            {
                return true; // Hot not full, room available
            }

            // Is this price better than worst hot level?
            Price worstHotBid = hotBids_.back().first;
            return price > worstHotBid;
        }
        else
        {
            // For asks, check if in top maxHotLevels_ by price
            if (hotAsks_.empty())
            {
                return true; // No hot levels yet, always add to hot
            }
            if (hotAsks_.size() < maxHotLevels_)
            {
                return true; // Hot not full, room available
            }

            // Is this price better than worst hot level?
            Price worstHotAsk = hotAsks_.back().first;
            return price < worstHotAsk;
        }
    }

    // Promotes a price level from cold storage to hot path.
    void HybridOrderBook::promoteToHot(Price price, bool isBuy)
    {
        if (isBuy)
        {
            auto coldIt = coldBids_.find(price);
            if (coldIt == coldBids_.end())
            {
                return;
            }

            // Make room if needed
            if (hotBids_.size() >= maxHotLevels_)
            {
                demoteFromHot(true);
            }

            // Find insertion point in hot vector
            auto hotIt = std::lower_bound(
                hotBids_.begin(), hotBids_.end(), price,
                [](const auto &priceLevel, Price p)
                { return priceLevel.first > p; });

            // Insert into hot
            auto insertIt = hotBids_.insert(hotIt, {price, std::move(coldIt->second)});
            std::size_t newIndex = insertIt - hotBids_.begin();

            // Update all orders at this price level in lookup
            for (auto &order : insertIt->second)
            {
                auto lookupIt = orderLookup_.find(order.id);
                if (lookupIt != orderLookup_.end())
                {
                    lookupIt->second.isHot = true;
                    lookupIt->second.vectorIndex = newIndex;
                }
            }

            // Update indices for orders after insertion point
            for (auto &pair : orderLookup_)
            {
                if (pair.second.isHot && pair.second.isBuy && pair.second.vectorIndex >= newIndex && orderLookup_.find(pair.first)->second.vectorIndex != newIndex)
                {
                    pair.second.vectorIndex++;
                }
            }

            coldBids_.erase(coldIt);
        }
        else
        {
            auto coldIt = coldAsks_.find(price);
            if (coldIt == coldAsks_.end())
            {
                return;
            }

            // Make room if needed
            if (hotAsks_.size() >= maxHotLevels_)
            {
                demoteFromHot(false);
            }

            // Find insertion point in hot vector
            auto hotIt = std::lower_bound(
                hotAsks_.begin(), hotAsks_.end(), price,
                [](const auto &priceLevel, Price p)
                { return priceLevel.first < p; });

            // Insert into hot
            auto insertIt = hotAsks_.insert(hotIt, {price, std::move(coldIt->second)});
            std::size_t newIndex = insertIt - hotAsks_.begin();

            // Update all orders at this price level in lookup
            for (auto &order : insertIt->second)
            {
                auto lookupIt = orderLookup_.find(order.id);
                if (lookupIt != orderLookup_.end())
                {
                    lookupIt->second.isHot = true;
                    lookupIt->second.vectorIndex = newIndex;
                }
            }

            // Update indices for orders after insertion point
            for (auto &pair : orderLookup_)
            {
                if (pair.second.isHot && !pair.second.isBuy && pair.second.vectorIndex >= newIndex && orderLookup_.find(pair.first)->second.vectorIndex != newIndex)
                {
                    pair.second.vectorIndex++;
                }
            }

            coldAsks_.erase(coldIt);
        }
    }

    // Demotes the worst price level from hot path to cold storage.
    void HybridOrderBook::demoteFromHot(bool isBuy)
    {
        if (isBuy)
        {
            if (hotBids_.empty())
            {
                return;
            }

            // Move worst (last) level to cold
            auto &worstLevel = hotBids_.back();
            Price worstPrice = worstLevel.first;

            coldBids_[worstPrice] = std::move(worstLevel.second);

            // Update lookup for all orders in this level
            for (auto &order : coldBids_[worstPrice])
            {
                auto lookupIt = orderLookup_.find(order.id);
                if (lookupIt != orderLookup_.end())
                {
                    lookupIt->second.isHot = false;
                    lookupIt->second.mapPrice = worstPrice;
                }
            }

            hotBids_.pop_back();
        }
        else
        {
            if (hotAsks_.empty())
            {
                return;
            }

            // Move worst (last) level to cold
            auto &worstLevel = hotAsks_.back();
            Price worstPrice = worstLevel.first;

            coldAsks_[worstPrice] = std::move(worstLevel.second);

            // Update lookup for all orders in this level
            for (auto &order : coldAsks_[worstPrice])
            {
                auto lookupIt = orderLookup_.find(order.id);
                if (lookupIt != orderLookup_.end())
                {
                    lookupIt->second.isHot = false;
                    lookupIt->second.mapPrice = worstPrice;
                }
            }

            hotAsks_.pop_back();
        }
    }

    // Adds order to hot path vector.
    void HybridOrderBook::addToHot(const Order &order, bool isBuy)
    {
        if (isBuy)
        {
            auto it = std::lower_bound(
                hotBids_.begin(), hotBids_.end(), order.price,
                [](const auto &priceLevel, Price price)
                { return priceLevel.first > price; });

            if (it != hotBids_.end() && it->first == order.price)
            {
                it->second.push_back(order);
                orderLookup_[order.id] = {true, true, {static_cast<std::size_t>(it - hotBids_.begin())}, --it->second.end()};
            }
            else
            {
                auto insertIt = hotBids_.insert(it, {order.price, OrderList{}});
                insertIt->second.push_back(order);
                std::size_t idx = insertIt - hotBids_.begin();
                orderLookup_[order.id] = {true, true, {idx}, --insertIt->second.end()};

                // Update indices for orders after insertion
                for (auto &pair : orderLookup_)
                {
                    if (pair.second.isHot && pair.second.isBuy && pair.second.vectorIndex >= idx && pair.first != order.id)
                    {
                        pair.second.vectorIndex++;
                    }
                }
            }
        }
        else
        {
            auto it = std::lower_bound(
                hotAsks_.begin(), hotAsks_.end(), order.price,
                [](const auto &priceLevel, Price price)
                { return priceLevel.first < price; });

            if (it != hotAsks_.end() && it->first == order.price)
            {
                it->second.push_back(order);
                orderLookup_[order.id] = {false, true, {static_cast<std::size_t>(it - hotAsks_.begin())}, --it->second.end()};
            }
            else
            {
                auto insertIt = hotAsks_.insert(it, {order.price, OrderList{}});
                insertIt->second.push_back(order);
                std::size_t idx = insertIt - hotAsks_.begin();
                orderLookup_[order.id] = {false, true, {idx}, --insertIt->second.end()};

                // Update indices for orders after insertion
                for (auto &pair : orderLookup_)
                {
                    if (pair.second.isHot && !pair.second.isBuy && pair.second.vectorIndex >= idx && pair.first != order.id)
                    {
                        pair.second.vectorIndex++;
                    }
                }
            }
        }
    }

    // Adds order to cold storage map.
    void HybridOrderBook::addToCold(const Order &order, bool isBuy)
    {
        OrderLocation loc;
        loc.isBuy = isBuy;
        loc.isHot = false;
        loc.mapPrice = order.price;

        if (isBuy)
        {
            auto &orderList = coldBids_[order.price];
            orderList.push_back(order);
            loc.iterator = --orderList.end();
        }
        else
        {
            auto &orderList = coldAsks_[order.price];
            orderList.push_back(order);
            loc.iterator = --orderList.end();
        }

        orderLookup_[order.id] = loc;
    }

    // Public APIS for future GUI
    std::size_t HybridOrderBook::getOrderCount() const
    {
        return orderLookup_.size();
    }

    Price HybridOrderBook::getBestBid() const
    {
        if (!hotBids_.empty())
        {
            return hotBids_.front().first;
        }
        if (!coldBids_.empty())
        {
            return coldBids_.begin()->first;
        }
        return 0; // No bids in the book
    }

    Price HybridOrderBook::getBestAsk() const
    {
        if (!hotAsks_.empty())
        {
            return hotAsks_.front().first;
        }
        if (!coldAsks_.empty())
        {
            return coldAsks_.begin()->first;
        }
        return std::numeric_limits<Price>::max(); // No asks in the book
    }

} // namespace hft
