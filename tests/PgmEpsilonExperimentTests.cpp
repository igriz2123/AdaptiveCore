#include "../src/benchmark/PgmEpsilonExperiment.h"
#include "../src/index/PGMIndex.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <string>

void test_pgm_diagnostic_metrics() {
    PGMIndex index(1);
    index.bulk_load({{0, "zero"}, {1, "one"}, {2, "two"}, {100, "hundred"}});

    assert(index.error_bound() == 1);
    assert(index.segment_count() == index.segments().size());
    assert(index.segment_count() == 2);
    assert(index.average_prediction_error() >= 0.0);
    assert(index.max_prediction_error() >= index.average_prediction_error());
    assert(index.model_memory_bytes() ==
           index.segment_count() * sizeof(PGMIndex::PiecewiseLinearSegment));
}

void test_epsilon_experiment_shape_and_determinism() {
    const auto first = PgmEpsilonExperiment::run();
    const auto second = PgmEpsilonExperiment::run();
    assert(first.size() == 84);
    assert(second.size() == first.size());

    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index].dataset_size == second[index].dataset_size);
        assert(first[index].dataset == second[index].dataset);
        assert(first[index].epsilon == second[index].epsilon);
        assert(first[index].operation == second[index].operation);
        assert(first[index].operation_count == second[index].operation_count);
        assert(first[index].segment_count == second[index].segment_count);
        assert(first[index].model_memory_bytes == second[index].model_memory_bytes);
        assert(first[index].average_prediction_error ==
               second[index].average_prediction_error);
        assert(first[index].max_prediction_error ==
               second[index].max_prediction_error);
        assert(first[index].bulk_load_nanoseconds >= 0);
        assert(first[index].total_nanoseconds >= 0);
        assert(std::isfinite(first[index].average_prediction_error));
        assert(std::isfinite(first[index].max_prediction_error));
    }

    for (std::size_t index = 0; index < first.size(); index += 2) {
        assert(first[index].operation == "BulkLoad");
        assert(first[index].operation_count == first[index].dataset_size);
        assert(first[index + 1].operation == "PointLookup");
        assert(first[index + 1].operation_count == first[index].dataset_size);
        assert(first[index].epsilon >= 1);
        assert(first[index].epsilon <= 64);
    }
}

void test_epsilon_csv_header() {
    const auto records = PgmEpsilonExperiment::run();
    const std::string path = "pgm_epsilon_test.csv";
    PgmEpsilonExperiment::write_csv(path, records);

    std::ifstream input(path);
    std::string header;
    std::getline(input, header);
    assert(header.find("DatasetSize,Dataset,Epsilon,Index,Operation") == 0);
    assert(header.find("SegmentCount") != std::string::npos);
    assert(header.find("AveragePredictionError") != std::string::npos);
    assert(header.find("ModelMemoryBytes") != std::string::npos);
}

int main() {
    test_pgm_diagnostic_metrics();
    test_epsilon_experiment_shape_and_determinism();
    test_epsilon_csv_header();
    return 0;
}
