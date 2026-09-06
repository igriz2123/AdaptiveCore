#include "BenchmarkRunner.h"
#include "DatasetGenerator.h"
#include "../index/BPlusTree.h"
#include "../index/HashIndex.h"

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Workload {
    std::string name;
    std::vector<int> keys;
};

std::vector<Index::Entry> make_entries(const std::vector<int>& keys) {
    std::vector<Index::Entry> entries;
    entries.reserve(keys.size());
    for (const int key : keys) {
        entries.emplace_back(key, "value-" + std::to_string(key));
    }
    return entries;
}

std::vector<BenchmarkRunner::RangeQuery> make_range_queries(
    const std::vector<int>& keys) {
    std::vector<BenchmarkRunner::RangeQuery> queries;
    queries.reserve(keys.size());
    for (const int key : keys) {
        queries.emplace_back(key - 3, key + 3);
    }
    return queries;
}

void print_result(const BenchmarkResult& result) {
    std::cout << "  " << result.operation_name
              << " | operations: " << result.operation_count
              << " | total ns: " << result.total_elapsed.count()
              << " | average ns/op: "
              << result.average_elapsed_per_operation.count() << '\n';
}

void run_workload(
    const std::string& index_name,
    const Workload& workload,
    const std::function<std::unique_ptr<Index>()>& create_index) {
    auto index = create_index();
    const auto entries = make_entries(workload.keys);
    const auto range_queries = make_range_queries(workload.keys);
    BenchmarkRunner runner;

    const auto insert_result = runner.run_insert(*index, entries);
    const auto lookup_result = runner.run_point_lookup(*index, workload.keys);
    const auto range_result = runner.run_range_query(*index, range_queries);
    const auto delete_result = runner.run_delete(*index, workload.keys);

    std::cout << "Index: " << index_name << " | Dataset: " << workload.name
              << '\n';
    print_result(insert_result);
    print_result(lookup_result);
    print_result(range_result);
    print_result(delete_result);
    std::cout << '\n';
}

std::size_t parse_dataset_size(int argc, char* argv[]) {
    if (argc == 1) {
        return 64;
    }
    if (argc != 2) {
        throw std::invalid_argument("usage: AdaptiveCoreBenchmark [dataset_size]");
    }

    const auto parsed_size = std::stoull(argv[1]);
    if (parsed_size == 0) {
        throw std::invalid_argument("dataset_size must be greater than zero");
    }
    return static_cast<std::size_t>(parsed_size);
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::size_t dataset_size = parse_dataset_size(argc, argv);
        const auto clustered_width = static_cast<int>(
            (dataset_size + 3) / 4);

        const std::vector<Workload> workloads = {
            {"Sequential", DatasetGenerator::generate_sequential(dataset_size,
                                                                  1000)},
            {"Random", DatasetGenerator::generate_random(
                            dataset_size, 2026, 100000, 100000 +
                                                       static_cast<int>(dataset_size * 10))},
            {"Clustered", DatasetGenerator::generate_clustered(
                               dataset_size, 3030, 4, clustered_width,
                               clustered_width + 100, 5000)}};

        std::cout << "AdaptiveCore benchmark, dataset size: " << dataset_size
                  << "\n\n";
        for (const auto& workload : workloads) {
            run_workload("HashIndex", workload,
                         [] { return std::make_unique<HashIndex>(); });
            run_workload("BPlusTree", workload,
                         [] { return std::make_unique<BPlusTree>(); });
        }
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
