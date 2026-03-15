#include "order_generator.hpp"
#include <chrono>

namespace hft
{

std::vector<Order> OrderGenerator::generateTightSpread(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    std::vector<Price> buyPrices = {9995, 9996, 9997, 9998, 9999};
    std::vector<Price> sellPrices = {10001, 10002, 10003, 10004, 10005};

    for (size_t i = 0; i < count; ++i)
    {
        Side side = (uniform_dist_(rng_) < 0.5) ? Side::Buy : Side::Sell;
        Price price = side == Side::Buy ? buyPrices[i % buyPrices.size()] : sellPrices[i % sellPrices.size()];

        Order order;
        order.id = orderIdCounter_++;
        order.price = price;
        order.quantity = 100 + (i % 900);
        order.side = side;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

        orders.push_back(order);
    }

    return orders;
}

std::vector<Order> OrderGenerator::generateFixedLevels(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    // Define 20 fixed price levels (10 bid, 10 ask)
    std::vector<Price> buyPrices = {9991, 9992, 9993, 9994, 9995, 9996, 9997, 9998, 9999, 10000};
    std::vector<Price> sellPrices = {10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10009, 10010};

    // Phase 1: Pre-populate all 20 price levels (first 20 orders)
    for (size_t i = 0; i < 10 && i < count; ++i)
    {
        Order order;
        order.id = orderIdCounter_++;
        order.price = buyPrices[i];
        order.quantity = 100;
        order.side = Side::Buy;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);
        orders.push_back(order);
    }

    for (size_t i = 0; i < 10 && orders.size() < count; ++i)
    {
        Order order;
        order.id = orderIdCounter_++;
        order.price = sellPrices[i];
        order.quantity = 100;
        order.side = Side::Sell;
        order.type = OrderType::Limit;
        order.timestamp =
            static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + orders.size());
        orders.push_back(order);
    }

    // Phase 2: Remaining orders only hit existing levels (99%+ hit rate)
    for (size_t i = 20; i < count; ++i)
    {
        Side side = (uniform_dist_(rng_) < 0.5) ? Side::Buy : Side::Sell;

        // Select from pre-populated levels
        Price price;
        if (side == Side::Buy)
        {
            price = buyPrices[static_cast<size_t>(uniform_dist_(rng_) * buyPrices.size())];
        }
        else
        {
            price = sellPrices[static_cast<size_t>(uniform_dist_(rng_) * sellPrices.size())];
        }

        Order order;
        order.id = orderIdCounter_++;
        order.price = price;
        order.quantity = 100 + static_cast<Quantity>(uniform_dist_(rng_) * 900);
        order.side = side;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

        orders.push_back(order);
    }

    return orders;
}

std::vector<Order> OrderGenerator::generateDenseFull(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    Price minPrice = 9000;
    Price maxPrice = 11000;
    Price tickSize = 1;

    for (size_t i = 0; i < count; ++i)
    {
        Side side = (uniform_dist_(rng_) < 0.5) ? Side::Buy : Side::Sell;

        // Generate price uniformly across full range
        int numTicks = static_cast<int>((maxPrice - minPrice) / tickSize);
        int tickOffset = static_cast<int>(uniform_dist_(rng_) * numTicks);
        Price price = minPrice + tickOffset * tickSize;

        Order order;
        order.id = orderIdCounter_++;
        order.price = price;
        order.quantity = 100 + (i % 900);
        order.side = side;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

        orders.push_back(order);
    }

    return orders;
}

std::vector<Order> OrderGenerator::generateSparseExtreme(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    std::vector<Price> extremePrices = {
        5000,  6000,  7000,  8000, // Far below mid
        12000, 13000, 14000, 15000 // Far above mid
    };

    for (size_t i = 0; i < count; ++i)
    {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price = extremePrices[i % extremePrices.size()];

        Order order;
        order.id = orderIdCounter_++;
        order.price = price;
        order.quantity = 100 + (i % 900);
        order.side = side;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

        orders.push_back(order);
    }

    return orders;
}

std::vector<Order> OrderGenerator::generateUniformRandom(size_t count)
{
    return generateDenseFull(count); // Same logic essentially
}

std::vector<Order> OrderGenerator::generateWorstCaseFIFO(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    Price singlePrice = 10000;

    for (size_t i = 0; i < count; ++i)
    {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;

        Order order;
        order.id = orderIdCounter_++;
        order.price = singlePrice;
        order.quantity = 100;
        order.side = side;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

        orders.push_back(order);
    }

    return orders;
}

std::vector<Order> OrderGenerator::generateMixed(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    std::vector<Price> nearBuyPrices = {9995, 9996, 9997, 9998, 9999, 10000};
    std::vector<Price> nearSellPrices = {10001, 10002, 10003, 10004, 10005, 10006};
    std::vector<Price> deepBuyPrices = {9950, 9960, 9970, 9980, 9990};
    std::vector<Price> deepSellPrices = {10010, 10020, 10030, 10040, 10050};
    std::vector<Price> extremeBuyPrices = {9400, 9500, 9600};
    std::vector<Price> extremeSellPrices = {10400, 10500, 10600};
    std::vector<OrderId> activeOrders;

    for (size_t i = 0; i < count; ++i)
    {
        double draw = uniform_dist_(rng_);

        // Moderate cancellations make mixed distinct without collapsing into the
        // dedicated high_cancellation regime.
        if (!activeOrders.empty() && draw < 0.18)
        {
            size_t cancelIdx = static_cast<size_t>(uniform_dist_(rng_) * activeOrders.size());
            if (cancelIdx >= activeOrders.size())
            {
                cancelIdx = activeOrders.size() - 1;
            }

            Order cancelOrder;
            cancelOrder.id = activeOrders[cancelIdx];
            cancelOrder.quantity = 0;
            cancelOrder.price = 10000;
            cancelOrder.side = Side::Buy;
            cancelOrder.type = OrderType::Limit;
            cancelOrder.timestamp =
                static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

            activeOrders.erase(activeOrders.begin() + cancelIdx);
            orders.push_back(cancelOrder);
            continue;
        }

        Side side = (uniform_dist_(rng_) < 0.5) ? Side::Buy : Side::Sell;
        Price price = 10000;

        // 55% near-spread activity, 30% deeper resting liquidity, 15% extremes.
        double regime = uniform_dist_(rng_);
        if (regime < 0.55)
        {
            const auto &priceLevels = side == Side::Buy ? nearBuyPrices : nearSellPrices;
            size_t idx = static_cast<size_t>(uniform_dist_(rng_) * priceLevels.size());
            if (idx >= priceLevels.size())
            {
                idx = priceLevels.size() - 1;
            }
            price = priceLevels[idx];
        }
        else if (regime < 0.85)
        {
            const auto &priceLevels = side == Side::Buy ? deepBuyPrices : deepSellPrices;
            size_t idx = static_cast<size_t>(uniform_dist_(rng_) * priceLevels.size());
            if (idx >= priceLevels.size())
            {
                idx = priceLevels.size() - 1;
            }
            price = priceLevels[idx];
        }
        else
        {
            const auto &priceLevels = side == Side::Buy ? extremeBuyPrices : extremeSellPrices;
            size_t idx = static_cast<size_t>(uniform_dist_(rng_) * priceLevels.size());
            if (idx >= priceLevels.size())
            {
                idx = priceLevels.size() - 1;
            }
            price = priceLevels[idx];
        }

        Order order;
        order.id = orderIdCounter_++;
        order.price = price;
        order.quantity = 100 + static_cast<Quantity>(uniform_dist_(rng_) * 900);
        order.side = side;
        order.type = OrderType::Limit;
        order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

        activeOrders.push_back(order.id);
        orders.push_back(order);
    }

    return orders;
}

std::vector<Order> OrderGenerator::generateHighCancellation(size_t count)
{
    std::vector<Order> orders;
    orders.reserve(count);

    std::vector<Price> prices = {9995, 9996, 9997, 9998, 9999, 10001, 10002, 10003, 10004, 10005};
    std::vector<OrderId> activeOrders;

    for (size_t i = 0; i < count; ++i)
    {
        if (!activeOrders.empty() && uniform_dist_(rng_) < 0.5) // If there are active orders, cancel one
        {
            size_t cancelIdx = static_cast<size_t>(uniform_dist_(rng_) * activeOrders.size());
            OrderId cancelId = activeOrders[cancelIdx];
            activeOrders.erase(activeOrders.begin() + cancelIdx);

            Order cancelOrder;
            cancelOrder.id = cancelId;
            cancelOrder.quantity =
                0; // IMPORTANT: A quantity of 0 acts as a flag to the Benchmark runner that this is a Cancel Order
            cancelOrder.price = 10000;
            cancelOrder.side = Side::Buy;
            cancelOrder.type = OrderType::Limit;
            cancelOrder.timestamp =
                static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

            orders.push_back(cancelOrder);
        }
        else // If no active orders, add a new order
        {
            Side side = (uniform_dist_(rng_) < 0.5) ? Side::Buy : Side::Sell;
            Price price = prices[static_cast<size_t>(uniform_dist_(rng_) * prices.size())];

            Order order;
            order.id = orderIdCounter_++;
            order.price = price;
            order.quantity = 100;
            order.side = side;
            order.type = OrderType::Limit;
            order.timestamp = static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count() + i);

            activeOrders.push_back(order.id);
            orders.push_back(order);
        }
    }

    return orders;
}

} // namespace hft
