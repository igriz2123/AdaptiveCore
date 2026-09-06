#include "../src/benchmark/BenchmarkRunner.h"
#include "../src/index/BPlusTree.h"
#include "../src/index/HashIndex.h"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

void assert_result(const BenchmarkResult& result,
                   const std::string& operation_name,
                   std::size_t operation_count) {
    assert(result.operation_name == operation_name);
    assert(result.operation_count == operation_count);
    assert(result.total_elapsed >= std::chrono::nanoseconds::zero());
    assert(result.average_elapsed_per_operation >=
           std::chrono::nanoseconds::zero());
    if (operation_count == 0) {
        assert(result.total_elapsed == std::chrono::nanoseconds::zero());
        assert(result.average_elapsed_per_operation ==
               std::chrono::nanoseconds::zero());
    } else {
        assert(result.average_elapsed_per_operation ==
               result.total_elapsed / operation_count);
    }
}

template <typename IndexType>
void test_runner_with_index() {
    IndexType index;
    BenchmarkRunner runner;
    const std::vector<Index::Entry> entries = {
        {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}};
    const std::vector<int> keys = {1, 3, 99};
    const std::vector<BenchmarkRunner::RangeQuery> queries = {{1, 2}, {2, 4}};

    const auto insert_result = runner.run_insert(index, entries);
    assert_result(insert_result, "Insert", entries.size());

    const auto lookup_result = runner.run_point_lookup(index, keys);
    assert_result(lookup_result, "PointLookup", keys.size());
    assert(index.find(1).value() == "one");
    assert(index.find(4).value() == "four");

    const auto range_result = runner.run_range_query(index, queries);
    assert_result(range_result, "RangeQuery", queries.size());

    const auto delete_result = runner.run_delete(index, {2, 4});
    assert_result(delete_result, "Delete", 2);
    assert(!index.find(2).has_value());
    assert(!index.find(4).has_value());
}

void test_empty_workloads() {
    HashIndex index;
    BenchmarkRunner runner;

    assert_result(runner.run_insert(index, {}), "Insert", 0);
    assert_result(runner.run_point_lookup(index, {}), "PointLookup", 0);
    assert_result(runner.run_range_query(index, {}), "RangeQuery", 0);
    assert_result(runner.run_delete(index, {}), "Delete", 0);
}

int main() {
    test_runner_with_index<HashIndex>();
    test_runner_with_index<BPlusTree>();
    test_empty_workloads();
    return 0;
}
