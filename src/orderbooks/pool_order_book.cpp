#include "pool_order_book.hpp"
#include <algorithm>

namespace hft
{
PoolOrderBook::PoolOrderBook(Index maxOrders)
{
    // Pre-allocate to prevent runtime allocations
    orders_.reserve(maxOrders);
    orderLookup_.reserve(maxOrders);

    orders_.resize(maxOrders);
    std::memset(orders_.data(), 0, maxOrders * sizeof(OrderNode));

    for (Index i = 0; i < maxOrders - 1; ++i)
    {
        orders_[i].nextFree = i + 1;
    }
    orders_[maxOrders - 1].nextFree = NULL_IDX;
    freeHead_ = 0;
}

void PoolOrderBook::addOrder(const Order &order)
{
    if (orderLookup_.find(order.id) != orderLookup_.end())
    {
        return; // Reject duplicate OrderId
    }

    // O(1) allocation from pool
    Index indexToAllocate = allocateSlot();
    orders_[indexToAllocate].order = order;
    orderLookup_[order.id] = indexToAllocate;

    if (order.side == Side::Buy)
    {
        appendToLevel(bids_, indexToAllocate, order.price);
    }
    else
    {
        appendToLevel(asks_, indexToAllocate, order.price);
    }
}

void PoolOrderBook::cancelOrder(OrderId orderId)
{
    auto it = orderLookup_.find(orderId);
    if (it == orderLookup_.end())
    {
        return;
    }

    Index idx = it->second;
    const Order &order = orders_[idx].order;

    if (order.side == Side::Buy)
    {
        removeFromLevel(bids_, idx);
    }
    else
    {
        removeFromLevel(asks_, idx);
    }

    orderLookup_.erase(it);
    freeSlot(idx);
}

void PoolOrderBook::modifyOrder(OrderId orderId, Quantity newQuantity)
{
    auto it = orderLookup_.find(orderId);
    if (it == orderLookup_.end())
    {
        return;
    }

    if (newQuantity == 0)
    {
        cancelOrder(orderId);
        return;
    }

    Index idx = it->second;
    orders_[idx].order.quantity = newQuantity;
}

std::vector<Trade> PoolOrderBook::match()
{
    std::vector<Trade> trades;
    trades.reserve(100);

    while (!bids_.empty() && !asks_.empty())
    {
        auto bestBidIt = bids_.begin();
        auto bestAskIt = asks_.begin();

        PoolLimitLevel &bidLevel = bestBidIt->second;
        PoolLimitLevel &askLevel = bestAskIt->second;

        // Check if the spread is crossed (Bid < Ask means no overlap)
        if (bidLevel.price < askLevel.price)
        {
            break; // No overlap, spread is open
        }

        Index bidIdx = bidLevel.head;
        Index askIdx = askLevel.head;

        // Iterate through orders at this price level.
        while (true)
        {

            Order &bid = orders_[bidIdx].order;
            Order &ask = orders_[askIdx].order;

            // Determine trade size (minimum of the two order quantities).
            Quantity quantity = std::min(bid.quantity, ask.quantity);

            trades.push_back({bid.id, ask.id, ask.price, quantity});

            bid.quantity -= quantity;
            ask.quantity -= quantity;

            Index nextBid = orders_[bidIdx].next;
            Index nextAsk = orders_[askIdx].next;

            Index bidToRemove = NULL_IDX;
            Index askToRemove = NULL_IDX;

            // Identify fully filled orders for removal.
            if (bid.quantity == 0)
            {
                bidToRemove = bidIdx;
                bidIdx = nextBid;
            }

            if (ask.quantity == 0)
            {
                askToRemove = askIdx;
                askIdx = nextAsk;
            }

            // Remove filled orders from the book and return slot to pool.
            if (bidToRemove != NULL_IDX)
            {
                removeFromLevel(bids_, bidToRemove);
                orderLookup_.erase(bid.id);
                freeSlot(bidToRemove);
            }

            if (askToRemove != NULL_IDX)
            {
                removeFromLevel(asks_, askToRemove);
                orderLookup_.erase(ask.id);
                freeSlot(askToRemove);
            }

            if (bids_.find(bidLevel.price) == bids_.end() || asks_.find(askLevel.price) == asks_.end())
            {
                break;
            }
        }
    }

    return trades;
}

template <typename MapType> void PoolOrderBook::appendToLevel(MapType &ladder, Index idx, Price price)
{
    auto &level = ladder[price];
    level.price = price;

    if (level.head == NULL_IDX)
    {
        level.head = idx;
        level.tail = idx;
        orders_[idx].prev = NULL_IDX;
        orders_[idx].next = NULL_IDX;
    }
    else
    {
        Index old_tail = level.tail;
        orders_[old_tail].next = idx;
        orders_[idx].prev = old_tail;
        orders_[idx].next = NULL_IDX;
        level.tail = idx;
    }
}

template <typename MapType> void PoolOrderBook::removeFromLevel(MapType &ladder, Index idx)
{
    Price price = orders_[idx].order.price;
    // Precondition: idx refers to an order currently linked into ladder at this price.
    auto levelIt = ladder.find(price);

    PoolLimitLevel &level = levelIt->second;

    Index prev = orders_[idx].prev;
    Index next = orders_[idx].next;

    if (prev != NULL_IDX) // We are removing head of the list so make next the new head
    {
        orders_[prev].next = next;
    }
    else
    {
        level.head = next;
    }

    if (next != NULL_IDX) // We are removing tail of the list so make prev the new tail
    {
        orders_[next].prev = prev;
    }
    else
    {
        level.tail = prev;
    }

    if (level.head == NULL_IDX) // We are removing the last element so erase the level
    {
        ladder.erase(levelIt);
    }
}

Index PoolOrderBook::allocateSlot()
{
    if (freeHead_ == NULL_IDX)
    {
        throw std::runtime_error("PoolOrderBook: Out of memory in pool!");
    }

    // Pop from free list (O(1))
    Index index = freeHead_;
    freeHead_ = orders_[index].nextFree;

    orders_[index].next = NULL_IDX;
    orders_[index].prev = NULL_IDX;
    orders_[index].nextFree = NULL_IDX;

    return index;
}

void PoolOrderBook::freeSlot(Index idx)
{
    // Push to free list (O(1))
    orders_[idx].nextFree = freeHead_;
    freeHead_ = idx;
}

Index PoolOrderBook::getOrderCount() const
{
    return orderLookup_.size();
}

Price PoolOrderBook::getBestBid() const
{
    if (bids_.empty())
        return 0;
    return bids_.begin()->first;
}

Price PoolOrderBook::getBestAsk() const
{
    if (asks_.empty())
        return std::numeric_limits<Price>::max();
    return asks_.begin()->first;
}

} // namespace hft
