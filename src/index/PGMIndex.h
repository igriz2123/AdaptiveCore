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

private:
    std::size_t error_bound_;
    std::vector<Entry> entries_;
    std::vector<PiecewiseLinearSegment> segments_;
};
