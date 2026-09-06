#include "PGMIndex.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

PGMIndex::PGMIndex(std::size_t error_bound)
    : error_bound_(error_bound) {}

void PGMIndex::insert(int, const std::string&) {
    throw std::logic_error("PGMIndex insertion is not implemented yet");
}

std::optional<std::string> PGMIndex::find(int key) const {
    if (entries_.empty() || segments_.empty()) {
        return std::nullopt;
    }

    const auto segment_position = std::upper_bound(
        segments_.begin(), segments_.end(), key,
        [](int searched_key, const PiecewiseLinearSegment& segment) {
            return searched_key < segment.first_key;
        });
    const auto& segment = segment_position == segments_.begin()
                              ? segments_.front()
                              : *std::prev(segment_position);

    const double predicted_position =
        segment.slope * static_cast<double>(key) + segment.intercept;
    const double error = static_cast<double>(error_bound_);
    const double raw_lower_position = std::floor(predicted_position - error);
    const double raw_upper_position = std::ceil(predicted_position + error);
    const double last_position = static_cast<double>(entries_.size() - 1);

    const std::size_t lower_position =
        raw_lower_position <= 0.0
            ? 0
            : raw_lower_position >= last_position
                  ? entries_.size() - 1
                  : static_cast<std::size_t>(raw_lower_position);
    const std::size_t upper_position =
        raw_upper_position <= 0.0
            ? 0
            : raw_upper_position >= last_position
                  ? entries_.size() - 1
                  : static_cast<std::size_t>(raw_upper_position);

    if (lower_position > upper_position) {
        return std::nullopt;
    }

    const auto begin = entries_.begin() + lower_position;
    const auto end = entries_.begin() + upper_position + 1;
    const auto entry = std::lower_bound(
        begin, end, key,
        [](const Entry& item, int searched_key) {
            return item.first < searched_key;
        });

    if (entry == end || entry->first != key) {
        return std::nullopt;
    }

    return entry->second;
}

bool PGMIndex::erase(int) {
    throw std::logic_error("PGMIndex deletion is not implemented yet");
}

std::vector<Index::Entry> PGMIndex::range(int, int) const {
    throw std::logic_error("PGMIndex range queries are not implemented yet");
}

std::size_t PGMIndex::size() const {
    return entries_.size();
}

std::size_t PGMIndex::error_bound() const {
    return error_bound_;
}

const std::vector<PGMIndex::PiecewiseLinearSegment>& PGMIndex::segments() const {
    return segments_;
}

void PGMIndex::load_sorted_entries_for_testing(
    const std::vector<Entry>& entries) {
    entries_ = entries;
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& left, const Entry& right) {
                  return left.first < right.first;
              });

    const auto duplicate = std::unique(
        entries_.begin(), entries_.end(),
        [](const Entry& left, const Entry& right) {
            return left.first == right.first;
        });
    entries_.erase(duplicate, entries_.end());
    rebuild_model();
}

void PGMIndex::rebuild_model() {
    segments_.clear();
    if (entries_.empty()) {
        return;
    }

    const auto make_segment = [this](std::size_t start,
                                     std::size_t end) {
        const int first_key = entries_[start].first;
        const int last_key = entries_[end].first;
        const auto position_span = static_cast<double>(end - start);
        const auto key_span = static_cast<double>(last_key - first_key);
        const double slope = start == end ? 0.0 : position_span / key_span;
        const double intercept = static_cast<double>(start) -
                                 slope * static_cast<double>(first_key);
        return PiecewiseLinearSegment{first_key, last_key, slope, intercept,
                                      start, end};
    };

    const auto fits = [this, &make_segment](std::size_t start,
                                            std::size_t end) {
        const auto segment = make_segment(start, end);
        const double allowed_error = static_cast<double>(error_bound_);
        for (std::size_t position = start; position <= end; ++position) {
            const double prediction =
                segment.slope * static_cast<double>(entries_[position].first) +
                segment.intercept;
            if (std::abs(prediction - static_cast<double>(position)) >
                allowed_error) {
                return false;
            }
        }
        return true;
    };

    std::size_t segment_start = 0;
    for (std::size_t segment_end = 1; segment_end < entries_.size();
         ++segment_end) {
        if (!fits(segment_start, segment_end)) {
            segments_.push_back(make_segment(segment_start, segment_end - 1));
            segment_start = segment_end;
        }
    }
    segments_.push_back(make_segment(segment_start, entries_.size() - 1));
}
