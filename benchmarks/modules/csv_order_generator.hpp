#pragma once

#include "core/order.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace hft
{

/**
 * @brief Generates orders from a CSV file.
 * Expected format: Side,Price,Quantity (e.g., Buy,100,10)
 */
class CSVOrderGenerator
{
  public:
    static std::vector<Order> parse(const std::string &filename)
    {
        std::vector<Order> orders;
        std::ifstream file(filename);
        std::string line;
        OrderId idCounter = 1;

        if (!file.is_open())
        {
            return orders;
        }

        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            std::stringstream ss(line);
            std::string sideStr, priceStr, qtyStr;

            if (std::getline(ss, sideStr, ',') && std::getline(ss, priceStr, ',') && std::getline(ss, qtyStr, ','))
            {

                Order order;
                order.id = idCounter++;
                order.side = (sideStr == "Buy" || sideStr == "1") ? Side::Buy : Side::Sell;
                order.price = std::stoull(priceStr);
                order.quantity = std::stoull(qtyStr);
                order.type = OrderType::Limit;
                order.timestamp = 0; // Filled by benchmark runner

                orders.push_back(order);
            }
        }
        return orders;
    }
};

} // namespace hft
