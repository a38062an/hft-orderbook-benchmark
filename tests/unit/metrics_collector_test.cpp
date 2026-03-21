#include <gtest/gtest.h>

#include "core/metrics_collector.hpp"

namespace hft
{
namespace
{

TEST(MetricsCollectorTest, EmptyStatsReturnZeros)
{
    MetricsCollector metrics;

    auto total = metrics.getStats();
    auto net = metrics.getNetworkStats();
    auto eng = metrics.getEngineStats();
    auto que = metrics.getQueueStats();

    EXPECT_EQ(total.mean, 0.0);
    EXPECT_EQ(total.p99, 0u);
    EXPECT_EQ(total.max, 0u);

    EXPECT_EQ(net.mean, 0.0);
    EXPECT_EQ(eng.mean, 0.0);
    EXPECT_EQ(que.mean, 0.0);
}

TEST(MetricsCollectorTest, RecordsSamplesAndCounters)
{
    MetricsCollector metrics;

    metrics.recordLatency(100);
    metrics.recordLatency(200);
    metrics.recordLatency(300);

    metrics.recordNetworkLatency(10);
    metrics.recordNetworkLatency(20);

    metrics.recordEngineLatency(30);
    metrics.recordEngineLatency(40);

    metrics.recordQueueLatency(50);
    metrics.recordQueueLatency(60);

    metrics.incrementOrders();
    metrics.incrementOrders();
    metrics.incrementTrades(3);

    auto total = metrics.getStats();
    auto net = metrics.getNetworkStats();
    auto eng = metrics.getEngineStats();
    auto que = metrics.getQueueStats();

    EXPECT_EQ(metrics.getOrderCount(), 2u);
    EXPECT_EQ(metrics.getTradeCount(), 3u);

    EXPECT_DOUBLE_EQ(total.mean, 200.0);
    EXPECT_EQ(total.max, 300u);
    EXPECT_EQ(total.p99, 300u);

    EXPECT_DOUBLE_EQ(net.mean, 15.0);
    EXPECT_EQ(net.max, 20u);

    EXPECT_DOUBLE_EQ(eng.mean, 35.0);
    EXPECT_EQ(eng.max, 40u);

    EXPECT_DOUBLE_EQ(que.mean, 55.0);
    EXPECT_EQ(que.max, 60u);
}

TEST(MetricsCollectorTest, CapacityGuardsStopAdditionalSamples)
{
    MetricsCollector metrics;
    constexpr uint64_t kCap = 1000000;

    for (uint64_t i = 1; i <= kCap + 1; ++i)
    {
        metrics.recordLatency(i);
        metrics.recordNetworkLatency(i);
        metrics.recordEngineLatency(i);
        metrics.recordQueueLatency(i);
    }

    auto total = metrics.getStats();
    auto net = metrics.getNetworkStats();
    auto eng = metrics.getEngineStats();
    auto que = metrics.getQueueStats();

    // If guard works, vectors stop at exactly kCap and ignore kCap+1 sample.
    EXPECT_DOUBLE_EQ(total.mean, (kCap + 1) / 2.0);
    EXPECT_DOUBLE_EQ(net.mean, (kCap + 1) / 2.0);
    EXPECT_DOUBLE_EQ(eng.mean, (kCap + 1) / 2.0);
    EXPECT_DOUBLE_EQ(que.mean, (kCap + 1) / 2.0);
    EXPECT_EQ(total.max, kCap);
    EXPECT_EQ(net.max, kCap);
    EXPECT_EQ(eng.max, kCap);
    EXPECT_EQ(que.max, kCap);
}

} // namespace
} // namespace hft
