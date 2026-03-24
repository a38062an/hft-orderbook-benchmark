#pragma once

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

namespace hft
{
#if defined(__linux__)
inline std::string formatAllowedCpus(const cpu_set_t &set)
{
    std::ostringstream oss;
    bool first_range = true;
    int range_start = -1;

    for (int cpu = 0; cpu <= CPU_SETSIZE; ++cpu)
    {
        const bool in_set = (cpu < CPU_SETSIZE) && CPU_ISSET(cpu, &set);
        if (in_set && range_start == -1)
        {
            range_start = cpu;
        }
        else if (!in_set && range_start != -1)
        {
            const int range_end = cpu - 1;
            if (!first_range)
            {
                oss << ",";
            }
            if (range_start == range_end)
            {
                oss << range_start;
            }
            else
            {
                oss << range_start << "-" << range_end;
            }
            first_range = false;
            range_start = -1;
        }
    }

    if (first_range)
    {
        return "<none>";
    }

    return oss.str();
}
#endif

/**
 * @brief Pin the current thread to a specific CPU core
 * @param core_id The core ID to pin to (0 to N-1)
 * @return true if successful, false otherwise
 */
inline bool pinToCore(int core_id)
{
#if defined(__linux__)
    cpu_set_t allowed_set;
    CPU_ZERO(&allowed_set);
    if (sched_getaffinity(0, sizeof(cpu_set_t), &allowed_set) != 0)
    {
        std::cerr << "Error: Failed to read allowed CPU mask: " << std::strerror(errno) << std::endl;
        return false;
    }

    if (core_id < 0 || core_id >= CPU_SETSIZE || !CPU_ISSET(core_id, &allowed_set))
    {
        std::cerr << "Error: Requested core " << core_id
                  << " is not allowed for this process. Allowed cores: " << formatAllowedCpus(allowed_set) << std::endl;
        return false;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    const int ret = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0)
    {
        std::cerr << "Error: Failed to pin thread to core " << core_id << ": " << std::strerror(ret) << std::endl;
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
