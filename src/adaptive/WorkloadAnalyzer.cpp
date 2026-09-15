#include "WorkloadAnalyzer.h"

#include <stdexcept>

WorkloadAnalyzer::WorkloadAnalyzer(std::size_t window_size)
    : window_size_(window_size) {
    if (window_size_ == 0) {
        throw std::invalid_argument("Workload window size must be greater than zero");
    }
}

void WorkloadAnalyzer::record(OperationType operation) {
    switch (operation) {
    case OperationType::Insert:
        ++snapshot_.inserts;
        break;
    case OperationType::PointLookup:
        ++snapshot_.point_lookups;
        break;
    case OperationType::RangeQuery:
        ++snapshot_.range_queries;
        break;
    case OperationType::Delete:
        ++snapshot_.deletes;
        break;
    }
    ++snapshot_.total_operations;
}

bool WorkloadAnalyzer::window_complete() const {
    return snapshot_.total_operations >= window_size_;
}

WorkloadSnapshot WorkloadAnalyzer::snapshot() const {
    auto result = snapshot_;
    if (result.total_operations == 0) {
        return result;
    }

    const auto total = static_cast<double>(result.total_operations);
    result.insert_ratio = static_cast<double>(result.inserts) / total;
    result.write_ratio = static_cast<double>(result.inserts + result.deletes) /
                         total;
    result.point_lookup_ratio = static_cast<double>(result.point_lookups) / total;
    result.range_query_ratio = static_cast<double>(result.range_queries) / total;
    result.delete_ratio = static_cast<double>(result.deletes) / total;
    return result;
}

void WorkloadAnalyzer::reset_window() {
    snapshot_ = {};
}

std::size_t WorkloadAnalyzer::window_size() const {
    return window_size_;
}
