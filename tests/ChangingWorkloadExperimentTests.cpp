#include "../src/adaptive/AdaptiveIndex.h"
#include "../src/benchmark/ChangingWorkloadExperiment.h"

#include <algorithm>
#include <cassert>
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
    assert(events[0].old_choice == IndexChoice::Hash);
    assert(events[0].new_choice == IndexChoice::PGM);
    assert(events[1].operation_number == 8);
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
    assert(std::count(row.begin(), row.end(), ',') == 14);
}

int main() {
    test_phase_boundaries();
    test_request_generation_is_deterministic();
    test_fixed_indexes_remain_fixed();
    test_adaptive_switch_events();
    test_csv_output();
    return 0;
}
