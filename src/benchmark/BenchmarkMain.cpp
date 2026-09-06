#include "BenchmarkRunner.h"
#include "DatasetGenerator.h"
#include "../index/BPlusTree.h"
#include "../index/HashIndex.h"
#include "../index/PGMIndex.h"

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
    std::size_t dataset_size,
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
              << " | Size: " << dataset_size << '\n';
    print_result(insert_result);
    print_result(lookup_result);
    print_result(range_result);
    print_result(delete_result);
    std::cout << '\n';
}

std::vector<Workload> make_workloads(std::size_t dataset_size) {
    const auto clustered_width = static_cast<int>((dataset_size + 3) / 4);

    return {
        {"Sequential", DatasetGenerator::generate_sequential(dataset_size,
                                                              1000)},
        {"Random", DatasetGenerator::generate_random(
                        dataset_size, 2026, 100000,
                        100000 + static_cast<int>(dataset_size * 10))},
        {"Clustered", DatasetGenerator::generate_clustered(
                           dataset_size, 3030, 4, clustered_width,
                           clustered_width + 100, 5000)}};
}

void run_size(std::size_t dataset_size) {
    std::cout << "Dataset size: " << dataset_size << "\n\n";
    for (const auto& workload : make_workloads(dataset_size)) {
        run_workload("HashIndex", workload, dataset_size,
                     [] { return std::make_unique<HashIndex>(); });
        run_workload("BPlusTree", workload, dataset_size,
                     [] { return std::make_unique<BPlusTree>(); });
        run_workload("PGMIndex", workload, dataset_size,
                     [] { return std::make_unique<PGMIndex>(); });
    }
}

std::vector<std::size_t> parse_dataset_sizes(int argc, char* argv[]) {
    if (argc == 1) {
        return {64};
    }
    if (argc != 2) {
        throw std::invalid_argument(
            "usage: AdaptiveCoreBenchmark [dataset_size|--experiment]");
    }

    if (std::string(argv[1]) == "--experiment") {
        return {100, 1000, 5000, 10000};
    }

    const auto parsed_size = std::stoull(argv[1]);
    if (parsed_size == 0) {
        throw std::invalid_argument("dataset_size must be greater than zero");
    }
    return {static_cast<std::size_t>(parsed_size)};
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto dataset_sizes = parse_dataset_sizes(argc, argv);
        std::cout << "AdaptiveCore benchmark\n\n";
        for (const auto dataset_size : dataset_sizes) {
            run_size(dataset_size);
        }
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
