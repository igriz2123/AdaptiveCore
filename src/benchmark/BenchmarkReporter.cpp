#include "BenchmarkReporter.h"

#include <fstream>
#include <stdexcept>

void BenchmarkReporter::write_csv(
    const std::string& output_path,
    const std::vector<BenchmarkRecord>& records) {
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Unable to open benchmark output file: " +
                                 output_path);
    }

    output << "DatasetSize,Index,Dataset,Operation,OperationCount,"
              "TotalNanoseconds,AverageNanoseconds\n";
    for (const auto& record : records) {
        output << record.dataset_size << ',' << record.index_name << ','
               << record.dataset_name << ',' << record.result.operation_name
               << ',' << record.result.operation_count << ','
               << record.result.total_elapsed.count() << ','
               << record.result.average_elapsed_per_operation.count() << '\n';
    }

    if (!output) {
        throw std::runtime_error("Unable to write benchmark output file: " +
                                 output_path);
    }
}
