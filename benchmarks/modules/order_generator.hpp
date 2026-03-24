#pragma once

#include <random>

#include "core/Order.hpp"
#include "core/Types.hpp"

namespace hft
{

/**
 * @brief Order generator for various market scenarios
 */
class OrderGenerator
{
  public:
    OrderGenerator(uint32_t seed = 12345) : rng_(seed), uniform_dist_(0.0, 1.0), orderIdCounter_(1)
    {
    }

    std::vector<Order> generateTightSpread(size_t count);
    std::vector<Order> generateFixedLevels(size_t count);
    std::vector<Order> generateDenseFull(size_t count);
    std::vector<Order> generateSparseExtreme(size_t count);
    std::vector<Order> generateUniformRandom(size_t count);
    std::vector<Order> generateWorstCaseFIFO(size_t count);
    std::vector<Order> generateMixed(size_t count);
    std::vector<Order> generateHighCancellation(size_t count);

    static std::vector<std::string> getSupportedScenarios()
    {
        return {"tight_spread",    "fixed_levels", "dense_full",       "sparse_extreme",
                "worst_case_fifo", "mixed",        "high_cancellation"};
    }

    std::vector<Order> generateScenario(const std::string &scenario, size_t count)
    {
        if (scenario == "tight_spread")
            return generateTightSpread(count);
        if (scenario == "fixed_levels")
            return generateFixedLevels(count);
        if (scenario == "dense_full")
            return generateDenseFull(count);
        if (scenario == "sparse_extreme")
            return generateSparseExtreme(count);
        if (scenario == "worst_case_fifo")
            return generateWorstCaseFIFO(count);
        if (scenario == "mixed")
            return generateMixed(count);
        if (scenario == "high_cancellation")
            return generateHighCancellation(count);
        return generateMixed(count);
    }

  private:
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_;
    OrderId orderIdCounter_;
};

} // namespace hft
