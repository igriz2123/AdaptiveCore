#pragma once

#include "../adaptive/AdaptiveIndex.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ChangingWorkloadRequest {
    OperationType operation;
    int key;
    int upper_key;
    std::string value;
};

struct ChangingWorkloadPhase {
    std::size_t number;
    std::string name;
    std::size_t begin_operation;
    std::size_t end_operation;
};

struct ChangingWorkloadConfig {
    std::array<std::size_t, 5> phase_lengths{{256, 256, 256, 256, 256}};
    std::uint32_t seed = 20260915;
    int key_space = 10000;
};

struct ChangingWorkloadRecord {
    std::string record_type;
    std::size_t phase;
    std::string phase_name;
    std::size_t phase_begin_operation;
    std::size_t phase_end_operation;
    std::string system;
    std::string active_index;
    std::size_t operation_count;
    std::int64_t total_nanoseconds;
    std::int64_t average_nanoseconds;
    std::size_t switch_count;
    std::size_t switch_operation_number;
    std::string switch_from;
    std::string switch_to;
    std::int64_t switch_duration_nanoseconds;
    double insert_ratio;
    double point_lookup_ratio;
    double range_query_ratio;
    double delete_ratio;
    std::size_t switch_window_number;
    std::size_t observed_key_count;
    std::size_t distinct_key_count;
    int minimum_key;
    int maximum_key;
    std::int64_t key_span;
    double key_mean;
    double key_variance;
    double key_monotonicity;
    double key_concentration;
};

class ChangingWorkloadExperiment {
public:
    static std::vector<ChangingWorkloadPhase> make_phases(
        const ChangingWorkloadConfig& config);

    static std::vector<ChangingWorkloadRequest> generate_requests(
        const ChangingWorkloadConfig& config);

    static std::vector<ChangingWorkloadRecord> run(
        const ChangingWorkloadConfig& config);

    static void write_csv(const std::string& output_path,
                          const std::vector<ChangingWorkloadRecord>& records);
};
