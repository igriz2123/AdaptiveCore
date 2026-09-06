#pragma once

#include "BenchmarkRunner.h"

#include <cstddef>
#include <string>
#include <vector>

struct BenchmarkRecord {
    std::size_t dataset_size;
    std::string index_name;
    std::string dataset_name;
    BenchmarkResult result;
};

class BenchmarkReporter {
public:
    static void write_csv(const std::string& output_path,
                          const std::vector<BenchmarkRecord>& records);
};
