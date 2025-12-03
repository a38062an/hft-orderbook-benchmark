#pragma once
#include <cstdint>

namespace hft 
{

inline uint64_t rdtsc() 
{
#ifdef __aarch64__
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#elif defined(__x86_64__) || defined(__i386__)
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
#else
    // Fallback for other architectures
    #include <chrono>
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

} // namespace hft
