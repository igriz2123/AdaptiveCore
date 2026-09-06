#include "AdaptiveIndex.h"

#include "../index/BPlusTree.h"
#include "../index/HashIndex.h"
#include "../index/PGMIndex.h"

#include <stdexcept>

AdaptiveIndex::AdaptiveIndex(
    std::size_t operation_window,
    std::size_t minimum_windows_between_switches,
    std::size_t bplus_tree_order,
    std::size_t pgm_error_bound)
    : active_index_(std::make_unique<HashIndex>()),
      current_choice_(IndexChoice::Hash),
      analyzer_(operation_window),
      minimum_windows_between_switches_(minimum_windows_between_switches),
      bplus_tree_order_(bplus_tree_order),
      pgm_error_bound_(pgm_error_bound),
      completed_windows_(0),
    windows_since_switch_(0) {}

void AdaptiveIndex::insert(int key, const std::string& value) {
    canonical_data_[key] = value;
    active_index_->insert(key, value);
    record_and_maybe_switch(OperationType::Insert);
}

std::optional<std::string> AdaptiveIndex::find(int key) const {
    const auto result = active_index_->find(key);
    record_and_maybe_switch(OperationType::PointLookup);
    return result;
}

bool AdaptiveIndex::erase(int key) {
    const auto existing = canonical_data_.find(key);
    if (existing == canonical_data_.end()) {
        return false;
    }

    canonical_data_.erase(existing);
    const bool erased = active_index_->erase(key);
    if (!erased) {
        return false;
    }
    record_and_maybe_switch(OperationType::Delete);
    return true;
}

std::vector<Index::Entry> AdaptiveIndex::range(int lower_key,
                                               int upper_key) const {
    const auto result = active_index_->range(lower_key, upper_key);
    record_and_maybe_switch(OperationType::RangeQuery);
    return result;
}

std::size_t AdaptiveIndex::size() const {
    return canonical_data_.size();
}

IndexChoice AdaptiveIndex::current_choice() const {
    return current_choice_;
}

WorkloadSnapshot AdaptiveIndex::current_window() const {
    return analyzer_.snapshot();
}

std::size_t AdaptiveIndex::completed_windows() const {
    return completed_windows_;
}

void AdaptiveIndex::record_and_maybe_switch(OperationType operation) const {
    analyzer_.record(operation);
    maybe_switch();
}

void AdaptiveIndex::maybe_switch() const {
    if (!analyzer_.window_complete()) {
        return;
    }

    const auto desired_choice = policy_.choose(analyzer_.snapshot());
    ++completed_windows_;
    ++windows_since_switch_;

    if (desired_choice != current_choice_ &&
        windows_since_switch_ >= minimum_windows_between_switches_) {
        active_index_ = rebuild_index(desired_choice);
        current_choice_ = desired_choice;
        windows_since_switch_ = 0;
    }

    analyzer_.reset_window();
}

std::unique_ptr<Index> AdaptiveIndex::create_index(IndexChoice choice) const {
    switch (choice) {
    case IndexChoice::Hash:
        return std::make_unique<HashIndex>();
    case IndexChoice::BPlusTree:
        return std::make_unique<BPlusTree>(bplus_tree_order_);
    case IndexChoice::PGM:
        return std::make_unique<PGMIndex>(pgm_error_bound_);
    }
    throw std::logic_error("Unknown adaptive index choice");
}

std::unique_ptr<Index> AdaptiveIndex::rebuild_index(IndexChoice choice) const {
    auto replacement = create_index(choice);
    if (choice == IndexChoice::PGM) {
        std::vector<Entry> entries;
        entries.reserve(canonical_data_.size());
        for (const auto& entry : canonical_data_) {
            entries.push_back(entry);
        }
        static_cast<PGMIndex*>(replacement.get())->bulk_load(entries);
    } else {
        for (const auto& [key, value] : canonical_data_) {
            replacement->insert(key, value);
        }
    }
    return replacement;
}
