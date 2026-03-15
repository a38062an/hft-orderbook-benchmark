#pragma once

#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

namespace hft
{
/**
 * @brief Pin the current thread to a specific CPU core
 * @param core_id The core ID to pin to (0 to N-1)
 * @return true if successful, false otherwise
 */
inline bool pinToCore(int core_id)
{
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0)
    {
        std::cerr << "Error: Failed to pin thread to core " << core_id << std::endl;
        return false;
    }
    return true;
#elif defined(__APPLE__)
    thread_affinity_policy_data_t policy = {core_id + 1}; // Tags are arbitrary integers
    thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());

    kern_return_t ret =
        thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);

    if (ret != KERN_SUCCESS)
    {
        std::cerr << "Error: Failed to set thread affinity tag " << core_id << " (Return code: " << ret
                  << "). Note: MacOS treats pinning as a hint." << std::endl;
        return false;
    }
    return true;
#else
    // Windows or other OS not implemented
    std::cerr << "Warning: Thread pinning not implemented for this OS" << std::endl;
    return false;
#endif
}
} // namespace hft
