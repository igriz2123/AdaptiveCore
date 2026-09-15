#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PgmEpsilonRecord {
    std::size_t dataset_size;
    std::string dataset;
    std::size_t epsilon;
    std::string index;
    std::string operation;
    std::size_t operation_count;
    std::int64_t total_nanoseconds;
    std::int64_t average_nanoseconds;
    std::size_t segment_count;
    std::int64_t bulk_load_nanoseconds;
    std::size_t model_memory_bytes;
    double average_prediction_error;
    double max_prediction_error;
};

class PgmEpsilonExperiment {
public:
    static std::vector<PgmEpsilonRecord> run();
    static void write_csv(const std::string& output_path,
                          const std::vector<PgmEpsilonRecord>& records);
};
