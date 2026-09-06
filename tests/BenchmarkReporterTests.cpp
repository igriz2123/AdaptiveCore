#include "../src/benchmark/BenchmarkReporter.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

BenchmarkRecord make_record(std::size_t dataset_size,
                            const std::string& index_name,
                            const std::string& dataset_name,
                            const std::string& operation_name,
                            std::size_t operation_count,
                            long long total_nanoseconds,
                            long long average_nanoseconds) {
    return {dataset_size,
            index_name,
            dataset_name,
            {operation_name,
             operation_count,
             std::chrono::nanoseconds(total_nanoseconds),
             std::chrono::nanoseconds(average_nanoseconds)}};
}

std::vector<std::string> read_lines(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return lines;
}

void test_header_and_single_record() {
    const auto path = std::filesystem::temp_directory_path() /
                      "adaptivecore_benchmark_reporter_test.csv";
    const std::vector<BenchmarkRecord> records = {
        make_record(32, "PGMIndex", "Random", "BulkLoad", 32, 640, 20)};

    BenchmarkReporter::write_csv(path.string(), records);
    const auto lines = read_lines(path);
    std::filesystem::remove(path);

    assert(lines.size() == 2);
    assert(lines[0] ==
           "DatasetSize,Index,Dataset,Operation,OperationCount,"
           "TotalNanoseconds,AverageNanoseconds");
    assert(lines[1] == "32,PGMIndex,Random,BulkLoad,32,640,20");
    std::stringstream fields(lines[1]);
    std::string field;
    std::size_t field_count = 0;
    while (std::getline(fields, field, ',')) {
        ++field_count;
    }
    assert(field_count == 7);
}

void test_multiple_records() {
    const auto path = std::filesystem::temp_directory_path() /
                      "adaptivecore_benchmark_reporter_multiple.csv";
    const std::vector<BenchmarkRecord> records = {
        make_record(10, "HashIndex", "Sequential", "Insert", 10, 100, 10),
        make_record(10, "BPlusTree", "Sequential", "Delete", 10, 200, 20)};

    BenchmarkReporter::write_csv(path.string(), records);
    const auto lines = read_lines(path);
    std::filesystem::remove(path);

    assert(lines.size() == 3);
    assert(lines[1] == "10,HashIndex,Sequential,Insert,10,100,10");
    assert(lines[2] == "10,BPlusTree,Sequential,Delete,10,200,20");
}

void test_empty_records_write_header_only() {
    const auto path = std::filesystem::temp_directory_path() /
                      "adaptivecore_benchmark_reporter_empty.csv";

    BenchmarkReporter::write_csv(path.string(), {});
    const auto lines = read_lines(path);
    std::filesystem::remove(path);

    assert(lines.size() == 1);
    assert(lines[0] ==
           "DatasetSize,Index,Dataset,Operation,OperationCount,"
           "TotalNanoseconds,AverageNanoseconds");
}

}  // namespace

int main() {
    test_header_and_single_record();
    test_multiple_records();
    test_empty_records_write_header_only();
    return 0;
}
