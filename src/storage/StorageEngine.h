#pragma once

#include "../index/HashIndex.h"

#include <memory>
#include <string>
#include <optional>

class StorageEngine {
public:
    StorageEngine();

    void put(int key, const std::string& value);

    std::optional<std::string> get(int key) const;

    bool remove(int key);

private:
    std::unique_ptr<Index> index_;
};