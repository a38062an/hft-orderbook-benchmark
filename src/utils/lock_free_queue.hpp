#pragma once

#include <cstddef>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <new>

namespace hft 
{

// Default to 64 cache-line padding where available
constexpr std::size_t CACHE_LINE_SIZE = 64;

template <typename ItemType, size_t QueueCapacity>
    requires(std::has_single_bit(QueueCapacity))
class LockFreeQueue
{
public:
    LockFreeQueue() 
        : writeIndex_{0}
        , readIndex_{0}
    {
    }

    // Disable copy / move operations to prevent state corruption
    LockFreeQueue(const LockFreeQueue &) = delete;
    LockFreeQueue &operator=(const LockFreeQueue &) = delete;
    LockFreeQueue(const LockFreeQueue &&) = delete;
    LockFreeQueue &operator=(const LockFreeQueue &&) = delete;

    [[nodiscard]] bool push(const ItemType &item) noexcept
    {
        const auto currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const auto nextWrite = currentWrite + 1;
        const auto currentRead = readIndex_.load(std::memory_order_acquire);

        // If the gap between the to write curosor and current read cursor is greater than capacity
        // then capacity has been reached (monotonic counters)
        if (nextWrite - currentRead > QueueCapacity)
        {
            return false;
        }

        // Bit-wise mask optimisation that only works because Capacity is a power of two
        buffer_[currentWrite & (QueueCapacity - 1)] = item;
        writeIndex_.store(nextWrite, std::memory_order_release);

        return true;
    }

    [[nodiscard]] bool pop(ItemType &value) noexcept
    {
        const auto currentRead = readIndex_.load(std::memory_order_relaxed);
        const auto currentWrite = writeIndex_.load(std::memory_order_acquire);

        // Means queue is empty
        if (currentRead == currentWrite)
        {
            return false;
        }

        value = buffer_[currentRead & (QueueCapacity - 1)];

        const auto nextRead = currentRead + 1;
        readIndex_.store(nextRead, std::memory_order_release);

        return true;
    }

    std::size_t size() const noexcept
    {
        size_t head = readIndex_.load(std::memory_order_acquire);
        size_t tail = writeIndex_.load(std::memory_order_acquire);

        return tail - head;
    }

    std::size_t capacity() const noexcept
    {
        return QueueCapacity;
    }

private:
    // Putting alignas on their own cache lines (64 bit per line)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> writeIndex_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> readIndex_;
    std::array<ItemType, QueueCapacity> buffer_;
};

} // namespace hft
