#include "../src/benchmark/MemoryFootprintExperiment.h"

#include <cassert>
#include <fstream>
#include <string>

void test_memory_experiment_shape() {
    const auto records = MemoryFootprintExperiment::run();
    assert(records.size() == 32);

    for (const auto& record : records) {
        assert(record.dataset_size == 1000 || record.dataset_size == 5000 ||
               record.dataset_size == 10000 || record.dataset_size == 50000);
        assert(record.dataset == "Sequential" || record.dataset == "Random");
        assert(record.index == "HashIndex" || record.index == "BPlusTree" ||
               record.index == "PGMIndex" || record.index == "AdaptiveIndex");
        assert(record.operation_count == record.dataset_size);
        assert(record.total_nanoseconds >= 0);
        assert(record.average_nanoseconds >= 0);
        assert(record.logical_payload_bytes > 0);
        assert(record.index_object_bytes > 0);
        assert(record.tracked_allocation_bytes >= 0);
        assert(record.peak_tracked_allocation_bytes >=
               record.tracked_allocation_bytes);
        assert(record.total_footprint_bytes >= record.index_object_bytes);
    }
}

void test_memory_csv_header() {
    const auto records = MemoryFootprintExperiment::run();
    const std::string path = "memory_footprint_test.csv";
    MemoryFootprintExperiment::write_csv(path, records);

    std::ifstream input(path);
    std::string header;
    std::getline(input, header);
    assert(header.find("DatasetSize,Dataset,Index,Operation,OperationCount") ==
           0);
    assert(header.find("LogicalPayloadBytes") != std::string::npos);
    assert(header.find("TrackedAllocationBytes") != std::string::npos);
    assert(header.find("TotalFootprintBytes") != std::string::npos);
}

int main() {
    test_memory_experiment_shape();
    test_memory_csv_header();
    return 0;
}
