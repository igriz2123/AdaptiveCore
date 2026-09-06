#pragma once

#include "../index/Index.h"

#include <chrono>
#include <cstddef>
#include <utility>
#include <vector>

struct BenchmarkResult {
    std::string operation_name;
    std::size_t operation_count;
    std::chrono::nanoseconds total_elapsed;
    std::chrono::nanoseconds average_elapsed_per_operation;
};

class BenchmarkRunner {
public:
    using RangeQuery = std::pair<int, int>;

    BenchmarkResult run_insert(
        Index& index,
        const std::vector<Index::Entry>& entries) const;

    BenchmarkResult run_point_lookup(Index& index,
                                     const std::vector<int>& keys) const;

    BenchmarkResult run_range_query(
        Index& index,
        const std::vector<RangeQuery>& queries) const;

    BenchmarkResult run_delete(Index& index,
                               const std::vector<int>& keys) const;
};
