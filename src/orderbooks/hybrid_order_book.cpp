#include "hybrid_order_book.hpp"
#include <algorithm>
#include <iostream>

namespace hft
{

HybridOrderBook::HybridOrderBook(Index maxHotLevels) : maxHotLevels_(maxHotLevels)
{
}

// Adds a new order to the book.
// Decides hot vs cold based on proximity to spread (top N price levels).
void HybridOrderBook::addOrder(const Order &order)
{
    if (orderLookup_.find(order.id) != orderLookup_.end())
    {
        return; // Reject duplicate OrderId
    }

    if (order.side == Side::Buy)
    {
        // Check if price exists in hot path
        auto hotIt = std::lower_bound(hotBids_.begin(), hotBids_.end(), order.price,
                                      [](const auto &priceLevel, Price price) { return priceLevel.first > price; });

        if (hotIt != hotBids_.end() && hotIt->first == order.price)
        {
            // Price level exists in hot, add there
            hotIt->second.push_back(order);
            orderLookup_[order.id] = {true, true, order.price, --hotIt->second.end()};
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
                loc.price = order.price;
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
        auto hotIt = std::lower_bound(hotAsks_.begin(), hotAsks_.end(), order.price,
                                      [](const auto &priceLevel, Price price) { return priceLevel.first < price; });

        if (hotIt != hotAsks_.end() && hotIt->first == order.price)
        {
            // Price level exists in hot, add there
            hotIt->second.push_back(order);
            orderLookup_[order.id] = {false, true, order.price, --hotIt->second.end()};
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
                loc.price = order.price;
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
            // Find price level via binary search (O(log K))
            auto levelIt =
                std::lower_bound(hotBids_.begin(), hotBids_.end(), loc.price,
                                 [](const auto &priceLevel, Price price) { return priceLevel.first > price; });

            auto &orderList = levelIt->second;
            orderList.erase(loc.iterator);

            // Clean up empty price levels (O(K))
            if (orderList.empty())
            {
                hotBids_.erase(levelIt);
                // No index updates needed! :)
            }
        }
        else
        {
            auto levelIt =
                std::lower_bound(hotAsks_.begin(), hotAsks_.end(), loc.price,
                                 [](const auto &priceLevel, Price price) { return priceLevel.first < price; });

            auto &orderList = levelIt->second;
            orderList.erase(loc.iterator);

            if (orderList.empty())
            {
                hotAsks_.erase(levelIt);
                // No index updates needed! :)
            }
        }
    }
    else
    {
        // Order in cold storage
        if (loc.isBuy)
        {
            auto mapIt = coldBids_.find(loc.price);
            mapIt->second.erase(loc.iterator);
            if (mapIt->second.empty())
            {
                coldBids_.erase(mapIt);
            }
        }
        else
        {
            auto mapIt = coldAsks_.find(loc.price);
            mapIt->second.erase(loc.iterator);
            if (mapIt->second.empty())
            {
                coldAsks_.erase(mapIt);
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

            trades.push_back({bidOrder.id, askOrder.id, askOrder.price, tradeQty});

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
            // No index updates needed
        }

        if (askOrderList.empty())
        {
            hotAsks_.erase(hotAsks_.begin());
            // No index updates needed
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
        // Precondition: price comes from existing coldBids_ best level in match().
        auto coldIt = coldBids_.find(price);

        // Make room if needed
        if (hotBids_.size() >= maxHotLevels_)
        {
            demoteFromHot(true);
        }

        // Called only from match() when hot side is empty; insertion at begin preserves ordering.
        auto insertIt = hotBids_.insert(hotBids_.begin(), {price, std::move(coldIt->second)});

        // Update all orders at this price level in lookup
        for (auto &order : insertIt->second)
        {
            auto lookupIt = orderLookup_.find(order.id);
            lookupIt->second.isHot = true;
            lookupIt->second.price = price; // Ensure price is set
        }

        coldBids_.erase(coldIt);
    }
    else
    {
        // Precondition: price comes from existing coldAsks_ best level in match().
        auto coldIt = coldAsks_.find(price);

        // Make room if needed
        if (hotAsks_.size() >= maxHotLevels_)
        {
            demoteFromHot(false);
        }

        // Called only from match() when hot side is empty; insertion at begin preserves ordering.
        auto insertIt = hotAsks_.insert(hotAsks_.begin(), {price, std::move(coldIt->second)});

        // Update all orders at this price level in lookup
        for (auto &order : insertIt->second)
        {
            auto lookupIt = orderLookup_.find(order.id);
            lookupIt->second.isHot = true;
            lookupIt->second.price = price; // Ensure price is set
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
            lookupIt->second.isHot = false;
            lookupIt->second.price = worstPrice;
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
            lookupIt->second.isHot = false;
            lookupIt->second.price = worstPrice;
        }

        hotAsks_.pop_back();
    }
}

// Adds order to hot path vector.
void HybridOrderBook::addToHot(const Order &order, bool isBuy)
{
    if (isBuy)
    {
        // Caller guarantees this price level is not already present in hotBids_.
        auto it = std::lower_bound(hotBids_.begin(), hotBids_.end(), order.price,
                                   [](const auto &priceLevel, Price price) { return priceLevel.first > price; });
        auto insertIt = hotBids_.insert(it, {order.price, OrderList{}});
        insertIt->second.push_back(order);
        orderLookup_[order.id] = {true, true, order.price, --insertIt->second.end()};
    }
    else
    {
        // Caller guarantees this price level is not already present in hotAsks_.
        auto it = std::lower_bound(hotAsks_.begin(), hotAsks_.end(), order.price,
                                   [](const auto &priceLevel, Price price) { return priceLevel.first < price; });
        auto insertIt = hotAsks_.insert(it, {order.price, OrderList{}});
        insertIt->second.push_back(order);
        orderLookup_[order.id] = {false, true, order.price, --insertIt->second.end()};
    }
}

// Adds order to cold storage map.
void HybridOrderBook::addToCold(const Order &order, bool isBuy)
{
    OrderLocation loc;
    loc.isBuy = isBuy;
    loc.isHot = false;
    loc.price = order.price;

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
Index HybridOrderBook::getOrderCount() const
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
