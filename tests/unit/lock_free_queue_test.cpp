#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "utils/lock_free_queue.hpp"

namespace hft
{
namespace
{

// -----------------------------------------------------------------------------
// Happy Path Cases
// -----------------------------------------------------------------------------

TEST(LockFreeQueueTest, HappyPath_PushPopFifo)
{
    LockFreeQueue<int, 8> queue;

    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));

    int out = 0;
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 1);
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 2);
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 3);
}

// -----------------------------------------------------------------------------
// Edge Cases
// -----------------------------------------------------------------------------

TEST(LockFreeQueueTest, EdgeCase_ReportsFullAtCapacity)
{
    LockFreeQueue<int, 4> queue;

    EXPECT_TRUE(queue.push(10));
    EXPECT_TRUE(queue.push(20));
    EXPECT_TRUE(queue.push(30));
    EXPECT_TRUE(queue.push(40));
    EXPECT_FALSE(queue.push(50));
    EXPECT_EQ(queue.size(), 4u);
}

TEST(LockFreeQueueTest, EdgeCase_WraparoundAfterPopAllowsPush)
{
    LockFreeQueue<int, 4> queue;
    int out = 0;

    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_TRUE(queue.push(4));
    EXPECT_FALSE(queue.push(5));

    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 1);
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 2);

    EXPECT_TRUE(queue.push(5));
    EXPECT_TRUE(queue.push(6));

    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 3);
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 4);
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 5);
    ASSERT_TRUE(queue.pop(out));
    EXPECT_EQ(out, 6);
}

TEST(LockFreeQueueTest, EdgeCase_SingleProducerSingleConsumerStress)
{
    constexpr int kItemCount = 10000;
    LockFreeQueue<int, 1024> queue;

    std::vector<int> consumed;
    consumed.reserve(kItemCount);

    std::thread producer(
        [&]()
        {
            for (int i = 0; i < kItemCount; ++i)
            {
                while (!queue.push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

    std::thread consumer(
        [&]()
        {
            int value = 0;
            while (static_cast<int>(consumed.size()) < kItemCount)
            {
                if (queue.pop(value))
                {
                    consumed.push_back(value);
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });

    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(kItemCount));
    for (int i = 0; i < kItemCount; ++i)
    {
        EXPECT_EQ(consumed[static_cast<size_t>(i)], i);
    }
}

// -----------------------------------------------------------------------------
// Error Cases
// -----------------------------------------------------------------------------

TEST(LockFreeQueueTest, ErrorCase_PopEmptyReturnsFalse)
{
    LockFreeQueue<int, 8> queue;
    int out = 0;
    EXPECT_FALSE(queue.pop(out));
}

} // namespace
} // namespace hft
