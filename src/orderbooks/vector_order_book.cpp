#include "vector_order_book.hpp"
#include <iostream>
#include <algorithm>

namespace hft
{

    // Adds a new order to the book.
    // Uses a std::vector sorted by price, with std::list for time priority at each level.
    void VectorOrderBook::addOrder(const Order &order)
    {
        if (order.side == Side::Buy)
        {
            // Binary search for price level (bids sorted high to low)
            auto bidIterator = std::lower_bound(
                bids_.begin(), bids_.end(), order.price,
                [](const auto &priceLevel, Price price)
                {
                    return priceLevel.first > price;
                });

            // If price level exists, add to existing list
            if (bidIterator != bids_.end() && bidIterator->first == order.price)
            {
                bidIterator->second.push_back(order);
                orderLookup_[order.id] = {true, order.price, --bidIterator->second.end()};
            }
            else
            {
                // Create new price level
                auto insertIterator = bids_.insert(bidIterator, {order.price, OrderList{}});
                insertIterator->second.push_back(order);
                orderLookup_[order.id] = {true, order.price, --insertIterator->second.end()};
            }
        }
        else
        {
            // Binary search for price level (asks sorted low to high)
            auto askIterator = std::lower_bound(
                asks_.begin(), asks_.end(), order.price,
                [](const auto &priceLevel, Price price)
                {
                    return priceLevel.first < price;
                });

            // If price level exists, add to existing list
            if (askIterator != asks_.end() && askIterator->first == order.price)
            {
                askIterator->second.push_back(order);
                orderLookup_[order.id] = {false, order.price, --askIterator->second.end()};
            }
            else
            {
                // Create new price level
                auto insertIterator = asks_.insert(askIterator, {order.price, OrderList{}});
                insertIterator->second.push_back(order);
                orderLookup_[order.id] = {false, order.price, --insertIterator->second.end()};
            }
        }
    }

    // Cancels an existing order by ID.
    // Uses the lookup table to find the order's location.
    void VectorOrderBook::cancelOrder(OrderId orderId)
    {
        auto orderIterator = orderLookup_.find(orderId);
        if (orderIterator == orderLookup_.end())
        {
            return; // Order not found
        }

        const auto &orderLocation = orderIterator->second;
        Price price = orderLocation.price;

        if (orderLocation.isBuy)
        {
            // Find price level via binary search
            auto levelIt = std::lower_bound(
                bids_.begin(), bids_.end(), price,
                [](const auto &priceLevel, Price p)
                {
                    return priceLevel.first > p;
                });

            if (levelIt != bids_.end() && levelIt->first == price)
            {
                auto &orderList = levelIt->second;
                orderList.erase(orderLocation.iterator);

                // Clean up empty price levels to keep vector size minimal
                if (orderList.empty())
                {
                    bids_.erase(levelIt);
                }
            }
        }
        else
        {
            // Find price level via binary search
            auto levelIt = std::lower_bound(
                asks_.begin(), asks_.end(), price,
                [](const auto &priceLevel, Price p)
                {
                    return priceLevel.first < p;
                });

            if (levelIt != asks_.end() && levelIt->first == price)
            {
                auto &orderList = levelIt->second;
                orderList.erase(orderLocation.iterator);

                // Clean up empty price levels
                if (orderList.empty())
                {
                    asks_.erase(levelIt);
                }
            }
        }

        orderLookup_.erase(orderIterator);
    }

    // Modifies the quantity of an existing order.
    void VectorOrderBook::modifyOrder(OrderId orderId, Quantity newQuantity)
    {
        auto orderIterator = orderLookup_.find(orderId);
        if (orderIterator == orderLookup_.end())
        {
            return;
        }

        // If modifying to zero quantity, cancel instead
        if (newQuantity == 0)
        {
            cancelOrder(orderId);
            return;
        }

        orderIterator->second.iterator->quantity = newQuantity;
    }

    // Matches buy and sell orders based on Price-Time priority.
    // Returns a vector of executed trades.
    std::vector<Trade> VectorOrderBook::match()
    {
        std::vector<Trade> trades;
        static uint64_t matchCounter{0};

        // Continue matching while there are overlapping prices
        while (!bids_.empty() && !asks_.empty())
        {
            auto &bestBid = bids_.front(); // Highest buy price
            auto &bestAsk = asks_.front(); // Lowest sell price

            // Check for price overlap (Bid >= Ask)
            if (bestBid.first < bestAsk.first)
            {
                break; // No overlap, spread is open
            }

            auto &bidOrderList = bestBid.second;
            auto &askOrderList = bestAsk.second;

            // Match orders at this price level
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
                              << "Price=" << bestAsk.first << " "
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
                bids_.erase(bids_.begin());
            }

            if (askOrderList.empty())
            {
                asks_.erase(asks_.begin());
            }
        }

        return trades;
    }

    // Public APIS for future GUI
    Index VectorOrderBook::getOrderCount() const
    {
        return orderLookup_.size();
    }

    Price VectorOrderBook::getBestBid() const
    {
        if (bids_.empty())
        {
            return 0; // No bids in the book
        }
        return bids_.front().first; // First element is highest price
    }

    Price VectorOrderBook::getBestAsk() const
    {
        if (asks_.empty())
        {
            return std::numeric_limits<Price>::max(); // No asks in the book
        }
        return asks_.front().first; // First element is lowest price
    }

} // namespace hft