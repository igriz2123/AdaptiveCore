#include "BenchmarkRunner.h"
#include "BenchmarkReporter.h"
#include "DatasetGenerator.h"
#include "../index/BPlusTree.h"
#include "../index/HashIndex.h"
#include "../index/PGMIndex.h"
#include "../adaptive/AdaptiveIndex.h"

#include <chrono>
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
    std::vector<BenchmarkRecord>& records,
    const std::function<std::unique_ptr<Index>()>& create_index) {
    auto index = create_index();
    const auto entries = make_entries(workload.keys);
    const auto range_queries = make_range_queries(workload.keys);
    BenchmarkRunner runner;

    const auto insert_result = runner.run_insert(*index, entries);
    records.push_back(
        {dataset_size, index_name, workload.name, insert_result});
    const auto lookup_result = runner.run_point_lookup(*index, workload.keys);
    records.push_back(
        {dataset_size, index_name, workload.name, lookup_result});
    const auto range_result = runner.run_range_query(*index, range_queries);
    records.push_back(
        {dataset_size, index_name, workload.name, range_result});
    const auto delete_result = runner.run_delete(*index, workload.keys);
    records.push_back(
        {dataset_size, index_name, workload.name, delete_result});

    std::cout << "Index: " << index_name << " | Dataset: " << workload.name
              << " | Size: " << dataset_size << '\n';
    print_result(insert_result);
    print_result(lookup_result);
    print_result(range_result);
    print_result(delete_result);

    std::cout << '\n';
}

void run_pgm_bulk_load(const Workload& workload,
                       std::size_t dataset_size,
                       std::vector<BenchmarkRecord>& records) {
    const auto entries = make_entries(workload.keys);
    PGMIndex bulk_index;
    BenchmarkRunner runner;
    const auto bulk_load_result = runner.run_custom(
        "BulkLoad", entries.size(),
        [&bulk_index, &entries] { bulk_index.bulk_load(entries); });
    std::cout << "Index: PGMIndex | Dataset: " << workload.name
              << " | Size: " << dataset_size << '\n';
    print_result(bulk_load_result);
    records.push_back(
        {dataset_size, "PGMIndex", workload.name, bulk_load_result});
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

void run_size(std::size_t dataset_size,
              std::vector<BenchmarkRecord>& records) {
    std::cout << "Running dataset size: " << dataset_size << '\n';
    for (const auto& workload : make_workloads(dataset_size)) {
        std::cout << "Running workload: " << workload.name << '\n';
        run_workload("HashIndex", workload, dataset_size, records,
                     [] { return std::make_unique<HashIndex>(); });
        run_workload("BPlusTree", workload, dataset_size, records,
                     [] { return std::make_unique<BPlusTree>(); });
        run_workload("PGMIndex", workload, dataset_size, records,
                     [] { return std::make_unique<PGMIndex>(); });
        run_workload("AdaptiveIndex", workload, dataset_size, records,
                 [] { return std::make_unique<AdaptiveIndex>(); });
        run_pgm_bulk_load(workload, dataset_size, records);
    }
}

struct BenchmarkOptions {
    std::vector<std::size_t> dataset_sizes;
    std::string output_path;
    bool experiment_mode;
};

BenchmarkOptions parse_options(int argc, char* argv[]) {
    BenchmarkOptions options{{64}, {}, false};
    bool size_was_provided = false;
    bool experiment_was_provided = false;

    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        const std::string argument = argv[argument_index];
        if (argument == "--experiment") {
            if (size_was_provided || experiment_was_provided) {
                throw std::invalid_argument("dataset mode specified more than once");
            }
            options.dataset_sizes = {100, 1000, 5000, 10000};
            options.experiment_mode = true;
            experiment_was_provided = true;
        } else if (argument == "--output") {
            if (argument_index + 1 >= argc ||
                std::string(argv[argument_index + 1]).empty()) {
                throw std::invalid_argument("--output requires a file path");
            }
            options.output_path = argv[++argument_index];
        } else if (!size_was_provided && !experiment_was_provided) {
            const auto parsed_size = std::stoull(argument);
            if (parsed_size == 0) {
                throw std::invalid_argument(
                    "dataset_size must be greater than zero");
            }
            options.dataset_sizes = {static_cast<std::size_t>(parsed_size)};
            size_was_provided = true;
        } else {
            throw std::invalid_argument(
                "usage: AdaptiveCoreBenchmark [dataset_size|--experiment] "
                "[--output output.csv]");
        }
    }

    return options;
}

}  // namespace

int main(int argc, char* argv[]) {
    BenchmarkOptions options{{64}, {}, false};
    std::vector<BenchmarkRecord> records;
    try {
        options = parse_options(argc, argv);
        const auto experiment_started = std::chrono::steady_clock::now();
        std::cout << "AdaptiveCore benchmark\n\n";
        for (const auto dataset_size : options.dataset_sizes) {
            run_size(dataset_size, records);
        }
        if (options.experiment_mode) {
            const auto experiment_finished = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(experiment_finished -
                                           experiment_started);
            std::cout << "Experiment wall-clock time: " << elapsed.count()
                      << " ms\n";
        }
        if (!options.output_path.empty()) {
            BenchmarkReporter::write_csv(options.output_path, records);
            std::cout << "CSV results written to: " << options.output_path
                      << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        if (!options.output_path.empty() && !records.empty()) {
            try {
                BenchmarkReporter::write_csv(options.output_path, records);
                std::cerr << "Partial CSV results written to: "
                          << options.output_path << '\n';
            } catch (const std::exception& report_error) {
                std::cerr << "Unable to write partial CSV results: "
                          << report_error.what() << '\n';
            }
        }
        return 1;
    }

    return 0;
}
