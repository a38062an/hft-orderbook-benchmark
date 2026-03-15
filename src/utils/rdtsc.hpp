#pragma once
#include <chrono>
#include <cstdint>

namespace hft
{

/**
 * @brief Returns current time in nanoseconds since epoch.
 * Uses std::chrono::high_resolution_clock for cross-platform portability.
 *
 * Note: this is NOT the x86 RDTSC CPU cycle counter. For true HFT production use,
 * replace with __builtin_ia32_rdtsc() combined with a known CPU frequency for ns
 * conversion. std::chrono is used here for macOS/Linux portability and dissertation
 * clarity — the overhead is acceptable for microbenchmark purposes.
 */
inline uint64_t getCurrentTimeNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

} // namespace hft
