#include "BenchmarkRunner.h"

namespace {

BenchmarkResult make_empty_result(const char* operation_name) {
    return {operation_name, 0, std::chrono::nanoseconds::zero(),
            std::chrono::nanoseconds::zero()};
}

BenchmarkResult make_result(const char* operation_name,
                            std::size_t operation_count,
                            std::chrono::steady_clock::time_point started,
                            std::chrono::steady_clock::time_point finished) {
    const auto total_elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started);
    const auto average_elapsed = operation_count == 0
                                     ? std::chrono::nanoseconds::zero()
                                     : total_elapsed /
                                           static_cast<std::chrono::nanoseconds::rep>(
                                               operation_count);

    return {operation_name, operation_count, total_elapsed, average_elapsed};
}

}  // namespace

BenchmarkResult BenchmarkRunner::run_insert(
    Index& index,
    const std::vector<Index::Entry>& entries) const {
    if (entries.empty()) {
        return make_empty_result("Insert");
    }

    const auto started = std::chrono::steady_clock::now();
    for (const auto& [key, value] : entries) {
        index.insert(key, value);
    }
    const auto finished = std::chrono::steady_clock::now();

    return make_result("Insert", entries.size(), started, finished);
}

BenchmarkResult BenchmarkRunner::run_point_lookup(
    Index& index,
    const std::vector<int>& keys) const {
    if (keys.empty()) {
        return make_empty_result("PointLookup");
    }

    volatile std::size_t observed_matches = 0;
    const auto started = std::chrono::steady_clock::now();
    for (const int key : keys) {
        if (index.find(key).has_value()) {
            ++observed_matches;
        }
    }
    const auto finished = std::chrono::steady_clock::now();

    return make_result("PointLookup", keys.size(), started, finished);
}

BenchmarkResult BenchmarkRunner::run_range_query(
    Index& index,
    const std::vector<RangeQuery>& queries) const {
    if (queries.empty()) {
        return make_empty_result("RangeQuery");
    }

    volatile std::size_t observed_entries = 0;
    const auto started = std::chrono::steady_clock::now();
    for (const auto& [lower_key, upper_key] : queries) {
        observed_entries += index.range(lower_key, upper_key).size();
    }
    const auto finished = std::chrono::steady_clock::now();

    return make_result("RangeQuery", queries.size(), started, finished);
}

BenchmarkResult BenchmarkRunner::run_delete(
    Index& index,
    const std::vector<int>& keys) const {
    if (keys.empty()) {
        return make_empty_result("Delete");
    }

    volatile std::size_t observed_deletions = 0;
    const auto started = std::chrono::steady_clock::now();
    for (const int key : keys) {
        if (index.erase(key)) {
            ++observed_deletions;
        }
    }
    const auto finished = std::chrono::steady_clock::now();

    return make_result("Delete", keys.size(), started, finished);
}
