#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

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
    std::size_t observed_key_count = 0;
    std::size_t distinct_key_count = 0;
    int minimum_key = 0;
    int maximum_key = 0;
    std::int64_t key_span = 0;
    double key_mean = 0.0;
    double key_variance = 0.0;
    double key_monotonicity = 0.0;
    double key_concentration = 0.0;
    double insert_ratio = 0.0;
    double write_ratio = 0.0;
    double point_lookup_ratio = 0.0;
    double range_query_ratio = 0.0;
    double delete_ratio = 0.0;
};

class WorkloadAnalyzer {
public:
    explicit WorkloadAnalyzer(std::size_t window_size = 64);

    void record(OperationType operation);
    void record(OperationType operation, int key);
    void record_range(int lower_key, int upper_key);
    bool window_complete() const;
    WorkloadSnapshot snapshot() const;
    void reset_window();
    std::size_t window_size() const;

private:
    void record_key_observation(int key);

    std::size_t window_size_;
    WorkloadSnapshot snapshot_;
    std::map<int, std::size_t> key_frequencies_;
    bool has_previous_key_ = false;
    int previous_key_ = 0;
    std::size_t nondecreasing_transitions_ = 0;
    int minimum_key_ = 0;
    int maximum_key_ = 0;
    double key_mean_ = 0.0;
    double key_m2_ = 0.0;
};
