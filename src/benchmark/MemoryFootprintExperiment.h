#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct MemoryFootprintRecord {
    std::size_t dataset_size;
    std::string dataset;
    std::string index;
    std::string operation;
    std::size_t operation_count;
    std::int64_t total_nanoseconds;
    std::int64_t average_nanoseconds;
    std::size_t logical_payload_bytes;
    std::size_t index_object_bytes;
    std::size_t tracked_allocation_bytes;
    std::size_t peak_tracked_allocation_bytes;
    std::size_t total_footprint_bytes;
};

class MemoryFootprintExperiment {
public:
    static std::vector<MemoryFootprintRecord> run();
    static void write_csv(const std::string& output_path,
                          const std::vector<MemoryFootprintRecord>& records);
};
