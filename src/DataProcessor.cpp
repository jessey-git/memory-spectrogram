#include "DataProcessor.h"
#include <map>
#include <algorithm>
#include <cmath>

static const std::vector<size_t> SIZE_BUCKETS = {
    8, 16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512,
    640, 768, 896, 1024, 1792, 2688, 4032, 5376, 8192, 16448, 24640, 32832, 41024,
    49216
};

DataProcessor::DataProcessor(double timeBucketMs)
    : timeBucketMs_(timeBucketMs)
{
}

const std::vector<size_t>& DataProcessor::getSizeBuckets()
{
    return SIZE_BUCKETS;
}

int DataProcessor::getSizeBucketIndex(size_t size)
{
    for (size_t i = 0; i < SIZE_BUCKETS.size(); ++i) {
        if (size <= SIZE_BUCKETS[i]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(SIZE_BUCKETS.size());
}

std::vector<BucketData> DataProcessor::procesData(const std::vector<AllocationEvent>& events)
{
    if (events.empty()) {
        return {};
    }
    
    double minTime = events[0].timeMs;
    
    std::map<std::pair<int, int>, int> bucketMap;
    
    for (const auto& event : events) {
        int timeBucket = static_cast<int>((event.timeMs - minTime) / timeBucketMs_);
        int sizeBucket = getSizeBucketIndex(event.size);
        
        auto key = std::make_pair(timeBucket, sizeBucket);
        bucketMap[key]++;
    }
    
    std::vector<BucketData> result;
    for (const auto& [key, count] : bucketMap) {
        int clampedCount = std::min(count, 100);
        result.push_back({key.first, key.second, clampedCount});
    }
    
    return result;
}
