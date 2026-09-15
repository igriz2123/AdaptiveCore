#pragma once

#include "AdaptivePolicy.h"
#include "IndexChoice.h"
#include "WorkloadAnalyzer.h"
#include "../index/Index.h"

#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct AdaptiveSwitchEvent {
    std::size_t operation_number;
    std::size_t window_number;
    IndexChoice old_choice;
    IndexChoice new_choice;
    std::chrono::nanoseconds duration;
    WorkloadSnapshot workload;
};

struct AdaptiveDecisionEvent {
    std::size_t operation_number;
    std::size_t window_number;
    IndexChoice active_choice;
    IndexChoice selected_choice;
    WorkloadSnapshot workload;
    bool switch_occurred;
    IndexChoice old_choice;
    IndexChoice new_choice;
    std::chrono::nanoseconds switch_duration;
};

class AdaptiveIndex final : public Index {
public:
    explicit AdaptiveIndex(
        std::size_t operation_window = 64,
        std::size_t minimum_windows_between_switches = 2,
        std::size_t bplus_tree_order = 4,
        std::size_t pgm_error_bound = 32);

    void insert(int key, const std::string& value) override;
    std::optional<std::string> find(int key) const override;
    bool erase(int key) override;
    std::vector<Entry> range(int lower_key, int upper_key) const override;
    std::size_t size() const override;

    IndexChoice current_choice() const;
    WorkloadSnapshot current_window() const;
    std::size_t completed_windows() const;
    std::size_t operation_count() const;
    std::size_t switch_count() const;
    std::vector<AdaptiveSwitchEvent> switch_events() const;
    std::vector<AdaptiveDecisionEvent> decision_events() const;

private:
    void record_and_maybe_switch(OperationType operation, int key) const;
    void record_range_and_maybe_switch(int lower_key, int upper_key) const;
    void maybe_switch() const;
    std::unique_ptr<Index> create_index(IndexChoice choice) const;
    std::unique_ptr<Index> rebuild_index(IndexChoice choice) const;

    std::map<int, std::string> canonical_data_;
    mutable std::unique_ptr<Index> active_index_;
    mutable IndexChoice current_choice_;
    mutable WorkloadAnalyzer analyzer_;
    AdaptivePolicy policy_;
    std::size_t minimum_windows_between_switches_;
    std::size_t bplus_tree_order_;
    std::size_t pgm_error_bound_;
    mutable std::size_t completed_windows_;
    mutable std::size_t windows_since_switch_;
    mutable std::size_t operation_count_;
    mutable std::vector<AdaptiveSwitchEvent> switch_events_;
    mutable std::vector<AdaptiveDecisionEvent> decision_events_;
};
