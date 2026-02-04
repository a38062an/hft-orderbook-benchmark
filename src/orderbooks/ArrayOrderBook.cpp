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
        if (order.side == Side::Buy)
        {
        }
        else
        {
        }
    }

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

}
