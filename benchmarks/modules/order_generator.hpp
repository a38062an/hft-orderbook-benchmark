#pragma once

#include <vector>
#include <random>
#include <chrono>

#include "../src/core/order.hpp"
#include "../src/core/types.hpp"

namespace hft {

/**
 * @brief Order generator for various market scenarios
 */
class OrderGenerator
{
public:
    OrderGenerator(uint32_t seed = 12345)
        : rng_(seed), uniform_dist_(0.0, 1.0), orderIdCounter_(1) {}

    std::vector<Order> generateTightSpread(size_t count);
    std::vector<Order> generateFixedLevels(size_t count);
    std::vector<Order> generateDenseFull(size_t count);
    std::vector<Order> generateSparseExtreme(size_t count);
    std::vector<Order> generateUniformRandom(size_t count);
    std::vector<Order> generateWorstCaseFIFO(size_t count);
    std::vector<Order> generateMixed(size_t count);
    std::vector<Order> generateHighCancellation(size_t count);

private:
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_;
    OrderId orderIdCounter_;
};

} // namespace hft
