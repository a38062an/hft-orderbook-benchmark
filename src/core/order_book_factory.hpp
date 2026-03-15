#pragma once

#include "../orderbooks/array_order_book.hpp"
#include "../orderbooks/hybrid_order_book.hpp"
#include "../orderbooks/map_order_book.hpp"
#include "../orderbooks/pool_order_book.hpp"
#include "../orderbooks/vector_order_book.hpp"
#include "i_order_book.hpp"
#include <memory>
#include <stdexcept>
#include <string>

namespace hft
{

class OrderBookFactory
{
  public:
    static std::vector<std::string> getSupportedTypes()
    {
        return {"map", "vector", "array", "hybrid", "pool"};
    }

    static std::unique_ptr<IOrderBook> create(const std::string &type)
    {
        if (type == "map")
        {
            return std::make_unique<MapOrderBook>();
        }
        else if (type == "vector")
        {
            return std::make_unique<VectorOrderBook>();
        }
        else if (type == "array")
        {
            // Default params for array book, can be made configurable
            return std::make_unique<ArrayOrderBook>(100, 200, 1);
        }
        else if (type == "hybrid")
        {
            return std::make_unique<HybridOrderBook>();
        }
        else if (type == "pool")
        {
            return std::make_unique<PoolOrderBook>();
        }

        throw std::runtime_error("Unknown OrderBook type: " + type);
    }
};

} // namespace hft
