#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>

namespace hft 
{

struct LatencyStats 
{
    uint64_t p50;
    uint64_t p99;
    uint64_t p999;
    uint64_t max;
    double mean;
};

class MetricsCollector 
{
public:
    void recordLatency(uint64_t cycles) 
    {
        samples_.push_back(cycles);
    }

    LatencyStats getStats() const 
    {
        if (samples_.empty()) 
        {
            return {};
        }
        
        std::vector<uint64_t> sortedSamples = samples_;
        std::sort(sortedSamples.begin(), sortedSamples.end());
        
        return {
            sortedSamples[sortedSamples.size() * 0.50],
            sortedSamples[sortedSamples.size() * 0.99],
            sortedSamples[sortedSamples.size() * 0.999],
            sortedSamples.back(),
            std::accumulate(sortedSamples.begin(), sortedSamples.end(), 0.0) / sortedSamples.size()
        };
    }

    void exportCSV(const std::string& filename) const 
    {
        std::ofstream file(filename);
        file << "latency_cycles\n";
        for (auto sample : samples_) 
        {
            file << sample << "\n";
        }
    }

private:
    std::vector<uint64_t> samples_;
};

} // namespace hft
