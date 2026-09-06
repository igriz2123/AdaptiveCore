#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Index {
public:
    using Entry = std::pair<int, std::string>;

    virtual ~Index() = default;

    virtual void insert(int key, const std::string& value) = 0;
    virtual std::optional<std::string> find(int key) const = 0;
    virtual bool erase(int key) = 0;
    virtual std::vector<Entry> range(int lower_key, int upper_key) const = 0;
    virtual std::size_t size() const = 0;
};