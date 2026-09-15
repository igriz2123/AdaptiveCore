#include "PgmEpsilonExperiment.h"

#include "BenchmarkRunner.h"
#include "DatasetGenerator.h"
#include "../index/PGMIndex.h"

#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct Dataset {
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

PgmEpsilonRecord make_record(const Dataset& dataset, std::size_t epsilon,
                             const std::string& operation,
                             const BenchmarkResult& result,
                             const PGMIndex& index,
                             std::int64_t bulk_load_nanoseconds) {
    return {dataset.keys.size(), dataset.name, epsilon, "PGM", operation,
            result.operation_count, result.total_elapsed.count(),
            result.average_elapsed_per_operation.count(), index.segment_count(),
            bulk_load_nanoseconds, index.model_memory_bytes(),
            index.average_prediction_error(), index.max_prediction_error()};
}

}  // namespace

std::vector<PgmEpsilonRecord> PgmEpsilonExperiment::run() {
    constexpr std::size_t dataset_sizes[] = {1000, 5000, 10000};
    constexpr std::size_t epsilons[] = {1, 2, 4, 8, 16, 32, 64};
    std::vector<PgmEpsilonRecord> records;
    BenchmarkRunner runner;

    for (const auto dataset_size : dataset_sizes) {
        const std::vector<Dataset> datasets = {
            {"Sequential", DatasetGenerator::generate_sequential(dataset_size,
                                                                   1000)},
            {"Random", DatasetGenerator::generate_random(
                            dataset_size, 2026, 100000,
                            100000 + static_cast<int>(dataset_size * 10))}};

        for (const auto& dataset : datasets) {
            const auto entries = make_entries(dataset.keys);
            for (const auto epsilon : epsilons) {
                PGMIndex index(epsilon);
                const auto bulk_load_result = runner.run_custom(
                    "BulkLoad", entries.size(),
                    [&index, &entries] { index.bulk_load(entries); });
                const auto lookup_result =
                    runner.run_point_lookup(index, dataset.keys);
                const auto bulk_load_nanoseconds =
                    bulk_load_result.total_elapsed.count();
                records.push_back(make_record(
                    dataset, epsilon, "BulkLoad", bulk_load_result, index,
                    bulk_load_nanoseconds));
                records.push_back(make_record(
                    dataset, epsilon, "PointLookup", lookup_result, index,
                    bulk_load_nanoseconds));
            }
        }
    }
    return records;
}

void PgmEpsilonExperiment::write_csv(
    const std::string& output_path,
    const std::vector<PgmEpsilonRecord>& records) {
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Unable to open PGM epsilon output file: " +
                                 output_path);
    }

    output << "DatasetSize,Dataset,Epsilon,Index,Operation,OperationCount,"
              "TotalNanoseconds,AverageNanoseconds,SegmentCount,"
              "BulkLoadNanoseconds,ModelMemoryBytes,AveragePredictionError,"
              "MaxPredictionError\n";
    for (const auto& record : records) {
        output << record.dataset_size << ',' << record.dataset << ','
               << record.epsilon << ',' << record.index << ','
               << record.operation << ',' << record.operation_count << ','
               << record.total_nanoseconds << ',' << record.average_nanoseconds
               << ',' << record.segment_count << ','
               << record.bulk_load_nanoseconds << ','
               << record.model_memory_bytes << ','
               << record.average_prediction_error << ','
               << record.max_prediction_error << '\n';
    }
    if (!output) {
        throw std::runtime_error("Unable to write PGM epsilon output file: " +
                                 output_path);
    }
}
