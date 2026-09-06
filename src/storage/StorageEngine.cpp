#include "StorageEngine.h"

StorageEngine::StorageEngine() : index_(std::make_unique<HashIndex>()) {}

void StorageEngine::put(int key, const std::string& value) {
    index_->insert(key, value);
}

std::optional<std::string> StorageEngine::get(int key) const {
    return index_->find(key);
}

bool StorageEngine::remove(int key) {
    return index_->erase(key);
}