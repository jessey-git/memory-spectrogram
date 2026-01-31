#pragma once

#include <vector>
#include <utility>

struct AllocationEvent {
    double timeMs;
    size_t size;
};

class DataSource {
public:
    virtual ~DataSource() = default;
    virtual std::vector<AllocationEvent> loadData() = 0;
};
