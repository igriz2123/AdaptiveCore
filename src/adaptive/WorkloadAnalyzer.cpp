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

void WorkloadAnalyzer::record(OperationType operation, int key) {
    record(operation);
    record_key_observation(key);
}

void WorkloadAnalyzer::record_key_observation(int key) {
    ++snapshot_.observed_key_count;
    ++key_frequencies_[key];
    if (!has_previous_key_) {
        has_previous_key_ = true;
        previous_key_ = key;
        minimum_key_ = key;
        maximum_key_ = key;
        key_mean_ = static_cast<double>(key);
        return;
    }

    if (key >= previous_key_) {
        ++nondecreasing_transitions_;
    }
    previous_key_ = key;
    if (key < minimum_key_) {
        minimum_key_ = key;
    }
    if (key > maximum_key_) {
        maximum_key_ = key;
    }

    const auto count = static_cast<double>(snapshot_.observed_key_count);
    const auto delta = static_cast<double>(key) - key_mean_;
    key_mean_ += delta / count;
    key_m2_ += delta * (static_cast<double>(key) - key_mean_);
}

void WorkloadAnalyzer::record_range(int lower_key, int upper_key) {
    record(OperationType::RangeQuery);
    record_key_observation(lower_key);
    record_key_observation(upper_key);
}

bool WorkloadAnalyzer::window_complete() const {
    return snapshot_.total_operations >= window_size_;
}

WorkloadSnapshot WorkloadAnalyzer::snapshot() const {
    auto result = snapshot_;
    if (result.observed_key_count != 0) {
        result.distinct_key_count = key_frequencies_.size();
        result.minimum_key = minimum_key_;
        result.maximum_key = maximum_key_;
        result.key_span = static_cast<std::int64_t>(maximum_key_) -
                          static_cast<std::int64_t>(minimum_key_);
        result.key_mean = key_mean_;
        result.key_variance = key_m2_ /
                              static_cast<double>(result.observed_key_count);
        if (result.observed_key_count == 1) {
            result.key_monotonicity = 1.0;
        } else {
            result.key_monotonicity =
                static_cast<double>(nondecreasing_transitions_) /
                static_cast<double>(result.observed_key_count - 1);
        }

        std::size_t most_frequent = 0;
        for (const auto& [key, frequency] : key_frequencies_) {
            (void)key;
            if (frequency > most_frequent) {
                most_frequent = frequency;
            }
        }
        result.key_concentration =
            static_cast<double>(most_frequent) /
            static_cast<double>(result.observed_key_count);
    }
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
    key_frequencies_.clear();
    has_previous_key_ = false;
    previous_key_ = 0;
    nondecreasing_transitions_ = 0;
    minimum_key_ = 0;
    maximum_key_ = 0;
    key_mean_ = 0.0;
    key_m2_ = 0.0;
}

std::size_t WorkloadAnalyzer::window_size() const {
    return window_size_;
}
