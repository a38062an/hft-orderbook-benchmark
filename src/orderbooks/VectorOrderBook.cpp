#include "VectorOrderBook.hpp"
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
            // "Should this element be LEFT of my search value in sorted order?" is what the lambda answers
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
                orderLookup_[order.id] = {true, static_cast<std::size_t>(bidIterator - bids_.begin()), --bidIterator->second.end()};
            }
            else
            {
                // Create new price level
                auto insertIterator = bids_.insert(bidIterator, {order.price, OrderList{}});
                insertIterator->second.push_back(order);
                orderLookup_[order.id] = {true, static_cast<std::size_t>(insertIterator - bids_.begin()), --insertIterator->second.end()};
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
                orderLookup_[order.id] = {false, static_cast<std::size_t>(askIterator - asks_.begin()), --askIterator->second.end()};
            }
            else
            {
                // Create new price level
                auto insertIterator = asks_.insert(askIterator, {order.price, OrderList{}});
                insertIterator->second.push_back(order);
                orderLookup_[order.id] = {false, static_cast<std::size_t>(insertIterator - asks_.begin()), --insertIterator->second.end()};
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
        std::size_t vectorIndex = orderLocation.vectorIndex;

        if (orderLocation.isBuy)
        {
            auto &orderList = bids_[vectorIndex].second;
            orderList.erase(orderLocation.iterator);

            // Clean up empty price levels to keep vector size minimal
            if (orderList.empty())
            {
                bids_.erase(bids_.begin() + vectorIndex);

                // Update lookup indices for orders after this level
                for (auto &pair : orderLookup_)
                {
                    if (pair.second.isBuy && pair.second.vectorIndex > vectorIndex)
                    {
                        pair.second.vectorIndex--;
                    }
                }
            }
        }
        else
        {
            auto &orderList = asks_[vectorIndex].second;
            orderList.erase(orderLocation.iterator);

            // Clean up empty price levels
            if (orderList.empty())
            {
                asks_.erase(asks_.begin() + vectorIndex);

                // Update lookup indices for orders after this level
                for (auto &pair : orderLookup_)
                {
                    if (!pair.second.isBuy && pair.second.vectorIndex > vectorIndex)
                    {
                        pair.second.vectorIndex--;
                    }
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

        // In a real book, increasing size might lose priority.
        // For simplicity here, we just update the quantity in place.
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

                // Update indices for remaining bid orders
                for (auto &pair : orderLookup_)
                {
                    if (pair.second.isBuy && pair.second.vectorIndex > 0)
                    {
                        pair.second.vectorIndex--;
                    }
                }
            }

            if (askOrderList.empty())
            {
                asks_.erase(asks_.begin());

                // Update indices for remaining ask orders
                for (auto &pair : orderLookup_)
                {
                    if (!pair.second.isBuy && pair.second.vectorIndex > 0)
                    {
                        pair.second.vectorIndex--;
                    }
                }
            }
        }

        return trades;
    }

    // Public APIS for future GUI
    std::size_t VectorOrderBook::getOrderCount() const
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