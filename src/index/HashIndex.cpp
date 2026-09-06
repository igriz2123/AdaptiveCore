#include "HashIndex.h"

#include <algorithm>

void HashIndex::insert(int key, const std::string& value) {
    data_[key] = value;
}

std::optional<std::string> HashIndex::find(int key) const {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool HashIndex::erase(int key) {
    return data_.erase(key) > 0;
}

std::vector<Index::Entry> HashIndex::range(int lower_key, int upper_key) const {
    std::vector<Entry> entries;

    if (lower_key > upper_key) {
        return entries;
    }

    for (const auto& [key, value] : data_) {
        if (key >= lower_key && key <= upper_key) {
            entries.emplace_back(key, value);
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const Entry& left, const Entry& right) {
                  return left.first < right.first;
              });
    return entries;
}

std::size_t HashIndex::size() const {
    return data_.size();
}