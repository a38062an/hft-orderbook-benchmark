#include "MapOrderBook.hpp"
#include <iostream>

namespace hft
{

    // Adds a new order to the book.
    // Uses a std::map for price levels (automatically sorted) and a std::list for time priority.
    void MapOrderBook::addOrder(const Order &order)
    {
        if (order.side == Side::Buy)
        {
            auto &orderList = bids_[order.price];
            orderList.push_back(order);
            // Store iterator for O(1) lookup/cancellation later
            orderLookup_[order.id] = {true, order.price, --orderList.end()};
        }
        else
        {
            auto &orderList = asks_[order.price];
            orderList.push_back(order);
            orderLookup_[order.id] = {false, order.price, --orderList.end()};
        }
    }

    // Cancels an existing order by ID.
    // Uses the lookup table to find the order's location in O(1) (average) or O(log N).
    void MapOrderBook::cancelOrder(OrderId orderId)
    {
        auto orderIterator = orderLookup_.find(orderId);
        if (orderIterator == orderLookup_.end())
        {
            return; // Order not found
        }

        const auto &orderLocation = orderIterator->second;
        if (orderLocation.isBuy)
        {
            auto &orderList = bids_[orderLocation.price]; // Get the list of that price
            orderList.erase(orderLocation.iterator);      // Delete the instruments in that price list
            // Clean up empty price levels to keep map size minimal
            if (orderList.empty())
            {
                bids_.erase(orderLocation.price);
            }
        }
        else
        {
            auto &orderList = asks_[orderLocation.price];
            orderList.erase(orderLocation.iterator);
            if (orderList.empty())
            {
                asks_.erase(orderLocation.price);
            }
        }
        orderLookup_.erase(orderIterator);
    }

    // Modifies the quantity of an existing order.
    void MapOrderBook::modifyOrder(OrderId orderId, Quantity newQuantity)
    {
        auto orderIterator = orderLookup_.find(orderId);
        if (orderIterator == orderLookup_.end())
        {
            return;
        }

        // In a real book, increasing size might lose priority.
        // For simplicity here, we just update the quantity in place.
        orderIterator->second.iterator->quantity = newQuantity;
    }

    // Matches buy and sell orders based on Price-Time priority.
    // Returns a vector of executed trades.
    std::vector<Trade> MapOrderBook::match()
    {
        std::vector<Trade> trades;
        static uint64_t matchCounter = 0;

        // Continue matching while there are overlapping prices
        while (!bids_.empty() && !asks_.empty())
        {
            auto bestBidIterator = bids_.begin(); // Highest buy price
            auto bestAskIterator = asks_.begin(); // Lowest sell price

            // Check for price overlap (Bid >= Ask)
            if (bestBidIterator->first < bestAskIterator->first)
            {
                break; // No overlap, spread is open
            }

            // Iterator to the best price lists
            auto &bidOrderList = bestBidIterator->second;
            auto &askOrderList = bestAskIterator->second;

            // Match orders at this price level
            while (!bidOrderList.empty() && !askOrderList.empty())
            {
                auto &bidOrder = bidOrderList.front();
                auto &askOrder = askOrderList.front();

                Quantity tradeQty = std::min(bidOrder.quantity, askOrder.quantity);

                trades.push_back({
                    bidOrder.id,
                    askOrder.id,
                    bestAskIterator->first,
                    tradeQty,
                    0 // Timestamp
                });

                matchCounter++;
                if (matchCounter % 1000 == 0)
                {
                    std::cout << "Match #" << matchCounter << ": "
                              << "Bid=" << bidOrder.id << " "
                              << "Ask=" << askOrder.id << " "
                              << "Price=" << bestAskIterator->first << " "
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
                bids_.erase(bestBidIterator);
            }
            if (askOrderList.empty())
            {
                asks_.erase(bestAskIterator);
            }
        }

        return trades;
    }

    std::size_t MapOrderBook::getOrderCount() const
    {
        return orderLookup_.size();
    }

} // namespace hft
