#pragma once

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/order.hpp"
#include "core/i_order_book.hpp"
#include "moodycamel/concurrentqueue.h"
#include "utils/rdtsc.hpp"

namespace hft
{

/**
 * @brief Results from a single MPSC benchmark run.
 */
struct MpscResult
{
    int producerCount;

    // Queue statistics (time from push to pop, measures producer contention)
    double queueMeanNs;
    uint64_t queueP99Ns;
    uint64_t queueMaxNs;

    // Engine statistics (time for addOrder + match, should be independent of producer count)
    double engineMeanNs;
    uint64_t engineP99Ns;

    // Throughput
    double throughputOrdersPerSec;

    // Reliability
    uint64_t ordersInjected;
    uint64_t ordersProcessed;
    uint64_t ordersDropped;  // push() returned false (queue full)
    uint64_t peakQueueDepth; // sampled max queue size during run
};

/**
 * @brief Runs a multi-producer, single-consumer benchmark using moodycamel::ConcurrentQueue.
 *
 * Architecture:
 *   N producer threads ──► ConcurrentQueue<Order> ──► 1 consumer thread (matching engine)
 *
 * Timing:
 *   - Each producer stamps order.sendTimestamp before enqueuing
 *   - Consumer stamps order.receiveTimestamp on dequeue
 *   - Queue latency = receiveTimestamp - sendTimestamp
 *   - Engine latency = addOrder + match duration
 */
class MpscBenchmark
{
  public:
    static constexpr size_t QUEUE_CAPACITY = 16384; // must be power of two (moodycamel default)

    static MpscResult run(const std::string & /*bookName*/, std::unique_ptr<IOrderBook> book,
                          const std::vector<Order> &orders, int producerCount, size_t warmupCount = 500)
    {
        moodycamel::ConcurrentQueue<Order> queue(QUEUE_CAPACITY);

        // Shared counters
        std::atomic<uint64_t> ordersConsumed{0};
        std::atomic<uint64_t> ordersDropped{0};
        std::atomic<uint64_t> peakDepth{0};
        std::atomic<bool> producersDone{false};

        // --- Per-order timing storage ---
        // We store queue latency per order consumed (safe: only consumer writes)
        const size_t measureOrders = orders.size() - warmupCount;
        std::vector<uint64_t> queueLatencies;
        std::vector<uint64_t> engineLatencies;
        queueLatencies.reserve(measureOrders);
        engineLatencies.reserve(measureOrders);

        // Partition orders across producers
        size_t totalOrders = orders.size();
        size_t sliceSize = (totalOrders + producerCount - 1) / producerCount;

        auto wallStart = std::chrono::high_resolution_clock::now();

        // --- Producer threads ---
        std::vector<std::thread> producers;
        producers.reserve(producerCount);

        for (int p = 0; p < producerCount; ++p)
        {
            size_t from = static_cast<size_t>(p) * sliceSize;
            size_t to = std::min(from + sliceSize, totalOrders);

            producers.emplace_back(
                [&, from, to]()
                {
                    moodycamel::ProducerToken token(queue);
                    for (size_t i = from; i < to; ++i)
                    {
                        Order o = orders[i];
                        o.sendTimestamp = getCurrentTimeNs(); // stamp before push
                        while (!queue.enqueue(token, o))
                        {
                            // Queue is full — spin briefly and retry
                            ordersDropped.fetch_add(1, std::memory_order_relaxed);
                            std::this_thread::yield();
                            o.sendTimestamp = getCurrentTimeNs(); // re-stamp after yield
                        }
                    }
                });
        }

        // --- Consumer thread (matching engine) ---
        uint64_t consumedTotal = 0;

        std::thread consumer(
            [&]()
            {
                Order o;
                while (consumedTotal < totalOrders)
                {
                    if (queue.try_dequeue(o))
                    {
                        uint64_t recvTs = getCurrentTimeNs();
                        o.receiveTimestamp = recvTs;

                        // Sample peak queue depth
                        size_t depth = queue.size_approx();
                        uint64_t prev = peakDepth.load(std::memory_order_relaxed);
                        while (depth > prev && !peakDepth.compare_exchange_weak(prev, depth))
                        {
                        }

                        // Engine timing
                        uint64_t engineStart = getCurrentTimeNs();
                        book->addOrder(o);
                        book->match();
                        uint64_t engineEnd = getCurrentTimeNs();

                        consumedTotal++;
                        ordersConsumed.fetch_add(1, std::memory_order_relaxed);

                        // Only record latency post-warmup
                        if (consumedTotal > warmupCount)
                        {
                            // Guard against stale/zero timestamps (unsigned underflow would create huge values)
                            if (o.sendTimestamp > 0 && recvTs > o.sendTimestamp)
                            {
                                uint64_t qLat = recvTs - o.sendTimestamp;
                                if (qLat < 100'000'000ULL) // sanity cap: 100ms max plausible queue latency
                                {
                                    queueLatencies.push_back(qLat);
                                }
                            }
                            engineLatencies.push_back(engineEnd - engineStart);
                        }
                    }
                    else if (producersDone.load(std::memory_order_acquire))
                    {
                        // Drain any remaining items
                        while (queue.try_dequeue(o))
                        {
                            uint64_t recvTs = getCurrentTimeNs();
                            uint64_t engineStart = getCurrentTimeNs();
                            book->addOrder(o);
                            book->match();
                            uint64_t engineEnd = getCurrentTimeNs();
                            consumedTotal++;
                            ordersConsumed.fetch_add(1, std::memory_order_relaxed);
                            if (consumedTotal > warmupCount && o.sendTimestamp > 0 && recvTs > o.sendTimestamp)
                            {
                                uint64_t qLat = recvTs - o.sendTimestamp;
                                if (qLat < 100'000'000ULL)
                                {
                                    queueLatencies.push_back(qLat);
                                }
                                engineLatencies.push_back(engineEnd - engineStart);
                            }
                        }
                        break;
                    }
                }
            });

        for (auto &t : producers)
            t.join();
        producersDone.store(true, std::memory_order_release);
        consumer.join();

        auto wallEnd = std::chrono::high_resolution_clock::now();
        double wallNs =
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count());

        // --- Compute stats ---
        auto calcStats = [](std::vector<uint64_t> &v) -> std::tuple<double, uint64_t, uint64_t>
        {
            if (v.empty())
                return {0.0, 0, 0};
            std::sort(v.begin(), v.end());
            double sum = 0;
            for (auto x : v)
                sum += x;
            double mean = sum / v.size();
            uint64_t p99 = v[static_cast<size_t>(v.size() * 0.99)];
            uint64_t max = v.back();
            return {mean, p99, max};
        };

        auto [qMean, qP99, qMax] = calcStats(queueLatencies);
        auto [eMean, eP99, eMax] = calcStats(engineLatencies);

        double throughput = (wallNs > 0) ? (totalOrders * 1e9 / wallNs) : 0.0;

        MpscResult result;
        result.producerCount = producerCount;
        result.queueMeanNs = qMean;
        result.queueP99Ns = qP99;
        result.queueMaxNs = qMax;
        result.engineMeanNs = eMean;
        result.engineP99Ns = eP99;
        result.throughputOrdersPerSec = throughput;
        result.ordersInjected = totalOrders;
        result.ordersProcessed = ordersConsumed.load();
        result.ordersDropped = ordersDropped.load();
        result.peakQueueDepth = peakDepth.load();

        return result;
    }
};

/**
 * @brief Print a formatted console summary of MPSC results.
 */
inline void printMpscTable(const std::string &book, const std::string &scenario, const std::vector<MpscResult> &results)
{
    std::cout << "\n" << std::string(100, '=') << "\n";
    std::cout << "MPSC BENCHMARK — Book: " << book << "  Scenario: " << scenario << "\n";
    std::cout << std::string(100, '=') << "\n";
    std::cout << std::left << std::setw(10) << "Producers" << std::right << std::setw(14) << "Que_Mean(ns)"
              << std::setw(14) << "Que_P99(ns)" << std::setw(14) << "Eng_Mean(ns)" << std::setw(14) << "Eng_P99(ns)"
              << std::setw(16) << "Throughput(k/s)" << std::setw(10) << "Dropped" << std::setw(12) << "PeakDepth"
              << "\n"
              << std::string(100, '-') << "\n";

    for (const auto &r : results)
    {
        std::cout << std::left << std::setw(10) << r.producerCount << std::right << std::fixed << std::setprecision(1)
                  << std::setw(14) << r.queueMeanNs << std::setw(14) << r.queueP99Ns << std::setw(14) << r.engineMeanNs
                  << std::setw(14) << r.engineP99Ns << std::setw(16)
                  << static_cast<uint64_t>(r.throughputOrdersPerSec / 1000) << std::setw(10) << r.ordersDropped
                  << std::setw(12) << r.peakQueueDepth << "\n";
    }
    std::cout << std::string(100, '=') << "\n";
}

} // namespace hft
