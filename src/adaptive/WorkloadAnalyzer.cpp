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
    return snapshot_;
}

void WorkloadAnalyzer::reset_window() {
    snapshot_ = {};
}

std::size_t WorkloadAnalyzer::window_size() const {
    return window_size_;
}
