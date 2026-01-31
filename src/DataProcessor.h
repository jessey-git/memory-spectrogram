#pragma once

#include "DataSource.h"
#include <vector>
#include <cstddef>

struct BucketData {
    int timeBucket;
    int sizeBucket;
    int allocationCount;
};

class DataProcessor {
public:
    explicit DataProcessor(double timeBucketMs = 5.0);
    
    std::vector<BucketData> procesData(const std::vector<AllocationEvent>& events);
    
    static int getSizeBucketIndex(size_t size);
    static const std::vector<size_t>& getSizeBuckets();
    
private:
    double timeBucketMs_;
};
