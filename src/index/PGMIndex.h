#pragma once

#include "Index.h"

#include <cstddef>
#include <vector>

class PGMIndex final : public Index {
public:
    struct PiecewiseLinearSegment {
        int first_key;
        int last_key;
        double slope;
        double intercept;
        std::size_t starting_position;
        std::size_t ending_position;
    };

    explicit PGMIndex(std::size_t error_bound = 32);

    void insert(int key, const std::string& value) override;
    std::optional<std::string> find(int key) const override;
    bool erase(int key) override;
    std::vector<Entry> range(int lower_key, int upper_key) const override;
    std::size_t size() const override;

    std::size_t error_bound() const;
    const std::vector<PiecewiseLinearSegment>& segments() const;
    std::size_t segment_count() const;
    double average_prediction_error() const;
    double max_prediction_error() const;
    std::size_t model_memory_bytes() const;

    void bulk_load(const std::vector<Entry>& entries);

    void load_sorted_entries_for_testing(const std::vector<Entry>& entries);
    std::size_t model_rebuild_count_for_testing() const;

private:
    void rebuild_model() const;

    std::size_t error_bound_;
    std::vector<Entry> entries_;
    mutable std::vector<PiecewiseLinearSegment> segments_;
    mutable std::size_t model_rebuild_count_;
    mutable bool model_dirty_;
};
