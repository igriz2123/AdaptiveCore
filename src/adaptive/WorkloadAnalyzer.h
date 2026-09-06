#pragma once

#include <cstddef>

enum class OperationType {
    Insert,
    PointLookup,
    RangeQuery,
    Delete
};

struct WorkloadSnapshot {
    std::size_t inserts = 0;
    std::size_t point_lookups = 0;
    std::size_t range_queries = 0;
    std::size_t deletes = 0;
    std::size_t total_operations = 0;
};

class WorkloadAnalyzer {
public:
    explicit WorkloadAnalyzer(std::size_t window_size = 64);

    void record(OperationType operation);
    bool window_complete() const;
    WorkloadSnapshot snapshot() const;
    void reset_window();
    std::size_t window_size() const;

private:
    std::size_t window_size_;
    WorkloadSnapshot snapshot_;
};
