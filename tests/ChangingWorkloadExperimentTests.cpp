#include "../src/adaptive/AdaptiveIndex.h"
#include "../src/benchmark/ChangingWorkloadExperiment.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <string>

void test_phase_boundaries() {
    ChangingWorkloadConfig config;
    config.phase_lengths = {{2, 3, 4, 5, 6}};
    const auto phases = ChangingWorkloadExperiment::make_phases(config);
    assert(phases.size() == 5);
    assert(phases[0].begin_operation == 0);
    assert(phases[0].end_operation == 2);
    assert(phases[1].begin_operation == 2);
    assert(phases[1].end_operation == 5);
    assert(phases[4].begin_operation == 14);
    assert(phases[4].end_operation == 20);
}

void test_request_generation_is_deterministic() {
    ChangingWorkloadConfig config;
    config.phase_lengths = {{3, 3, 3, 3, 3}};
    config.seed = 1234;
    const auto first = ChangingWorkloadExperiment::generate_requests(config);
    const auto second = ChangingWorkloadExperiment::generate_requests(config);
    assert(first.size() == 15);
    assert(first.size() == second.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index].operation == second[index].operation);
        assert(first[index].key == second[index].key);
        assert(first[index].upper_key == second[index].upper_key);
        assert(first[index].value == second[index].value);
    }
}

void test_fixed_indexes_remain_fixed() {
    ChangingWorkloadConfig config;
    config.phase_lengths = {{8, 8, 8, 8, 8}};
    const auto records = ChangingWorkloadExperiment::run(config);
    for (const auto& record : records) {
        if (record.record_type == "Summary" &&
            (record.system == "BPlusTree" || record.system == "PGMIndex")) {
            assert(record.switch_count == 0);
        }
    }
}

void test_experiment_measurements_are_deterministic() {
    ChangingWorkloadConfig config;
    config.phase_lengths = {{8, 8, 8, 8, 8}};
    const auto first = ChangingWorkloadExperiment::run(config);
    const auto second = ChangingWorkloadExperiment::run(config);
    assert(first.size() == second.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index].record_type == second[index].record_type);
        assert(first[index].phase == second[index].phase);
        assert(first[index].system == second[index].system);
        assert(first[index].operation_count == second[index].operation_count);
        assert(first[index].insert_ratio == second[index].insert_ratio);
        assert(first[index].point_lookup_ratio == second[index].point_lookup_ratio);
        assert(first[index].range_query_ratio == second[index].range_query_ratio);
        assert(first[index].delete_ratio == second[index].delete_ratio);
        assert(first[index].observed_key_count == second[index].observed_key_count);
        assert(first[index].distinct_key_count == second[index].distinct_key_count);
        assert(first[index].key_span == second[index].key_span);
        assert(first[index].key_concentration == second[index].key_concentration);
    }
}

void test_adaptive_switch_events() {
    AdaptiveIndex index(4, 0);
    for (int key = 0; key < 4; ++key) {
        index.find(key);
    }
    for (int key = 0; key < 4; ++key) {
        index.range(0, 3);
    }

    const auto events = index.switch_events();
    assert(index.switch_count() == 2);
    assert(events[0].operation_number == 4);
    assert(events[0].window_number == 1);
    assert(events[0].old_choice == IndexChoice::Hash);
    assert(events[0].new_choice == IndexChoice::PGM);
    assert(events[1].operation_number == 8);
    assert(events[1].window_number == 2);
    assert(events[1].old_choice == IndexChoice::PGM);
    assert(events[1].new_choice == IndexChoice::BPlusTree);
    assert(events[0].duration.count() >= 0);
    assert(events[1].duration.count() >= 0);
}

void test_csv_output() {
    ChangingWorkloadConfig config;
    config.phase_lengths = {{1, 1, 1, 1, 1}};
    const auto records = ChangingWorkloadExperiment::run(config);
    const std::string path = "changing_workload_test.csv";
    ChangingWorkloadExperiment::write_csv(path, records);

    std::ifstream input(path);
    std::string header;
    std::getline(input, header);
    assert(header.find("RecordType,Phase,PhaseName,System") == 0);
    std::string row;
    assert(std::getline(input, row));
    assert(row.find("Summary,") == 0);
    assert(std::count(row.begin(), row.end(), ',') ==
           std::count(header.begin(), header.end(), ','));
    assert(header.find("InsertRatio") != std::string::npos);
    assert(header.find("SwitchWindowNumber") != std::string::npos);
    assert(header.find("DistinctKeyCount") != std::string::npos);
    assert(header.find("KeyMonotonicity") != std::string::npos);
    assert(header.find("PhaseChangeOperation") != std::string::npos);
    assert(header.find("DetectionDelayOperations") != std::string::npos);
    assert(header.find("PreSwitchAverageNanoseconds") != std::string::npos);
    assert(header.find("OperationsPerSecond") != std::string::npos);
    assert(header.find("FalseSwitchRate") != std::string::npos);
}

void test_adaptation_effectiveness_metrics() {
    ChangingWorkloadConfig config;
    config.phase_lengths = {{64, 64, 64, 64, 64}};
    config.operation_window = 16;
    config.minimum_windows_between_switches = 0;
    config.switch_evaluation_window = 8;
    const auto records = ChangingWorkloadExperiment::run(config);

    std::size_t summaries = 0;
    std::size_t update_records = 0;
    std::size_t switches = 0;
    for (const auto& record : records) {
        if (record.record_type == "Summary") {
            ++summaries;
            if (record.system == "AdaptiveIndex" && record.phase > 1) {
                assert(record.phase_change_operation.has_value());
                assert(record.detection_operation.has_value());
                assert(record.detection_delay_operations.has_value());
                assert(*record.detection_operation > *record.phase_change_operation);
                assert(*record.detection_delay_operations >= config.operation_window);
                assert(*record.detection_delay_operations >= 0);
                assert(record.total_switches.has_value());
                assert(record.false_switch_count.has_value());
                assert(record.false_switch_rate.has_value());
                assert(*record.false_switch_rate >= 0.0);
                assert(*record.false_switch_rate <= 1.0);
                assert(record.adaptive_vs_bplus_percent.has_value());
                assert(record.adaptive_vs_pgm_percent.has_value());
            }
        } else if (record.record_type == "UpdateThroughput") {
            ++update_records;
            assert(record.measured_operation == "Insert" ||
                   record.measured_operation == "Delete");
            if (record.operation_count == 0) {
                assert(!record.operations_per_second.has_value());
            } else {
                assert(record.operations_per_second.has_value());
                assert(*record.operations_per_second > 0.0);
                const auto expected = static_cast<double>(record.operation_count) *
                                      1'000'000'000.0 / record.total_nanoseconds;
                assert(std::abs(*record.operations_per_second - expected) < 1e-6);
            }
        } else if (record.record_type == "Switch") {
            ++switches;
            assert(!record.switch_from.empty());
            assert(!record.switch_to.empty());
            assert(record.pre_switch_average_nanoseconds.has_value());
            assert(record.post_switch_average_nanoseconds.has_value());
            assert(record.false_switch.has_value());
        }
    }
    assert(summaries == 15);
    assert(update_records == 30);
    assert(switches > 0);
    assert(records.size() == summaries + update_records + switches);
}

int main() {
    test_phase_boundaries();
    test_request_generation_is_deterministic();
    test_fixed_indexes_remain_fixed();
    test_experiment_measurements_are_deterministic();
    test_adaptive_switch_events();
    test_csv_output();
    test_adaptation_effectiveness_metrics();
    return 0;
}
