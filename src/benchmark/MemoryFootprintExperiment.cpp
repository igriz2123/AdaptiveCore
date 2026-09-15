#include "MemoryFootprintExperiment.h"

#include "BenchmarkRunner.h"
#include "DatasetGenerator.h"
#include "../adaptive/AdaptiveIndex.h"
#include "../index/BPlusTree.h"
#include "../index/HashIndex.h"
#include "../index/PGMIndex.h"

#include <chrono>
#include <cstdlib>
#include <array>
#include <fstream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct AllocationSlot {
    void* pointer = nullptr;
    std::size_t size;
};

constexpr std::size_t allocation_table_size = 1U << 20;
std::array<AllocationSlot, allocation_table_size> allocation_table;
bool tracking_enabled = false;
std::size_t tracked_bytes = 0;
std::size_t peak_tracked_bytes = 0;

std::size_t allocation_hash(void* pointer) {
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    return (value >> 4U) & (allocation_table_size - 1U);
}

bool remember_allocation(void* pointer, std::size_t size) {
    auto slot = allocation_hash(pointer);
    for (std::size_t probe = 0; probe < allocation_table_size; ++probe) {
        auto& entry = allocation_table[slot];
        if (entry.pointer == nullptr || entry.pointer == pointer) {
            entry.pointer = pointer;
            entry.size = size;
            return true;
        }
        slot = (slot + 1U) & (allocation_table_size - 1U);
    }
    return false;
}

bool forget_allocation(void* pointer, std::size_t& size) {
    auto slot = allocation_hash(pointer);
    for (std::size_t probe = 0; probe < allocation_table_size; ++probe) {
        auto& entry = allocation_table[slot];
        if (entry.pointer == pointer) {
            size = entry.size;
            entry.pointer = nullptr;
            entry.size = 0;
            return true;
        }
        if (entry.pointer == nullptr) {
            return false;
        }
        slot = (slot + 1U) & (allocation_table_size - 1U);
    }
    return false;
}

void* allocate_bytes(std::size_t size) {
    const auto actual_size = size == 0 ? 1 : size;
    auto* pointer = std::malloc(actual_size);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    if (tracking_enabled) {
        if (!remember_allocation(pointer, actual_size)) {
            std::free(pointer);
            throw std::bad_alloc();
        }
        tracked_bytes += actual_size;
        if (tracked_bytes > peak_tracked_bytes) {
            peak_tracked_bytes = tracked_bytes;
        }
    }
    return pointer;
}

void release_bytes(void* pointer) noexcept {
    if (pointer == nullptr) {
        return;
    }
    std::size_t size = 0;
    if (tracking_enabled) {
        if (forget_allocation(pointer, size)) {
            tracked_bytes -= size;
        }
    }
    std::free(pointer);
}

class AllocationScope {
public:
    AllocationScope() {
        tracked_bytes = 0;
        peak_tracked_bytes = 0;
        tracking_enabled = true;
    }

    ~AllocationScope() {
        tracking_enabled = false;
    }

    void stop() {
        tracking_enabled = false;
    }

    std::size_t current_bytes() const {
        return tracked_bytes;
    }

    std::size_t peak_bytes() const {
        return peak_tracked_bytes;
    }
};

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

std::size_t logical_payload_bytes(const std::vector<Index::Entry>& entries) {
    std::size_t bytes = 0;
    for (const auto& [key, value] : entries) {
        (void)key;
        bytes += sizeof(int) + value.size() + 1;
    }
    return bytes;
}

template <typename IndexType, typename Populate>
MemoryFootprintRecord measure_index(const Dataset& dataset,
                                    const std::string& index_name,
                                    const std::string& operation,
                                    Populate populate,
                                    std::size_t operation_count,
                                    std::size_t payload_bytes) {
    AllocationScope allocation_scope;
    std::int64_t total_nanoseconds = 0;
    std::size_t tracked = 0;
    std::size_t peak = 0;
    {
        IndexType index;
        const auto started = std::chrono::steady_clock::now();
        populate(index);
        const auto finished = std::chrono::steady_clock::now();
        total_nanoseconds = std::chrono::duration_cast<
            std::chrono::nanoseconds>(finished - started)
                                .count();
        tracked = allocation_scope.current_bytes();
        peak = allocation_scope.peak_bytes();
    }
    allocation_scope.stop();
    const auto average_nanoseconds = operation_count == 0
                                         ? 0
                                         : total_nanoseconds /
                                               static_cast<std::int64_t>(
                                                   operation_count);
    return {dataset.keys.size(), dataset.name, index_name,
            operation, operation_count, total_nanoseconds,
            average_nanoseconds, payload_bytes, sizeof(IndexType), tracked,
            peak, tracked + sizeof(IndexType)};
}

}  // namespace

void* operator new(std::size_t size) {
    return allocate_bytes(size);
}

void* operator new[](std::size_t size) {
    return allocate_bytes(size);
}

void operator delete(void* pointer) noexcept {
    release_bytes(pointer);
}

void operator delete[](void* pointer) noexcept {
    release_bytes(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    release_bytes(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    release_bytes(pointer);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate_bytes(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate_bytes(size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    release_bytes(pointer);
}

std::vector<MemoryFootprintRecord> MemoryFootprintExperiment::run() {
    constexpr std::size_t dataset_sizes[] = {1000, 5000, 10000, 50000};
    std::vector<MemoryFootprintRecord> records;

    for (const auto dataset_size : dataset_sizes) {
        const std::vector<Dataset> datasets = {
            {"Sequential", DatasetGenerator::generate_sequential(dataset_size,
                                                                   1000)},
            {"Random", DatasetGenerator::generate_random(
                            dataset_size, 2026, 100000,
                            100000 + static_cast<int>(dataset_size * 10))}};

        for (const auto& dataset : datasets) {
            const auto entries = make_entries(dataset.keys);
            const auto payload_bytes = logical_payload_bytes(entries);
            BenchmarkRunner runner;
            (void)runner;

            auto hash_record = measure_index<HashIndex>(
                dataset, "HashIndex", "Insert",
                [&entries](HashIndex& index) {
                    for (const auto& entry : entries) {
                        index.insert(entry.first, entry.second);
                    }
                },
                entries.size(), payload_bytes);
            hash_record.index = "HashIndex";
            records.push_back(hash_record);

            auto bplus_record = measure_index<BPlusTree>(
                dataset, "BPlusTree", "Insert",
                [&entries](BPlusTree& index) {
                    for (const auto& entry : entries) {
                        index.insert(entry.first, entry.second);
                    }
                },
                entries.size(), payload_bytes);
            bplus_record.index = "BPlusTree";
            records.push_back(bplus_record);

            auto pgm_record = measure_index<PGMIndex>(
                dataset, "PGMIndex", "BulkLoad",
                [&entries](PGMIndex& index) { index.bulk_load(entries); },
                entries.size(), payload_bytes);
            pgm_record.index = "PGMIndex";
            records.push_back(pgm_record);

            auto adaptive_record = measure_index<AdaptiveIndex>(
                dataset, "AdaptiveIndex", "Insert",
                [&entries](AdaptiveIndex& index) {
                    for (const auto& entry : entries) {
                        index.insert(entry.first, entry.second);
                    }
                },
                entries.size(), payload_bytes);
            adaptive_record.index = "AdaptiveIndex";
            records.push_back(adaptive_record);
        }
    }
    return records;
}

void MemoryFootprintExperiment::write_csv(
    const std::string& output_path,
    const std::vector<MemoryFootprintRecord>& records) {
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Unable to open memory output file: " +
                                 output_path);
    }

    output << "DatasetSize,Dataset,Index,Operation,OperationCount,"
              "TotalNanoseconds,AverageNanoseconds,LogicalPayloadBytes,"
              "IndexObjectBytes,TrackedAllocationBytes,"
              "PeakTrackedAllocationBytes,TotalFootprintBytes\n";
    for (const auto& record : records) {
        output << record.dataset_size << ',' << record.dataset << ','
               << record.index << ',' << record.operation << ','
               << record.operation_count << ',' << record.total_nanoseconds
               << ',' << record.average_nanoseconds << ','
               << record.logical_payload_bytes << ','
               << record.index_object_bytes << ','
               << record.tracked_allocation_bytes << ','
               << record.peak_tracked_allocation_bytes << ','
               << record.total_footprint_bytes << '\n';
    }
    if (!output) {
        throw std::runtime_error("Unable to write memory output file: " +
                                 output_path);
    }
}
