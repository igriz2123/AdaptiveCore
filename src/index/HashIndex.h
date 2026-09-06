#pragma once

#include "Index.h"

#include <unordered_map>

class HashIndex final : public Index {
public:
    void insert(int key, const std::string& value) override;
    std::optional<std::string> find(int key) const override;
    bool erase(int key) override;
    std::vector<Entry> range(int lower_key, int upper_key) const override;
    std::size_t size() const override;

private:
    std::unordered_map<int, std::string> data_;
};