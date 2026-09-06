#include "../src/index/HashIndex.h"
#include "../src/storage/StorageEngine.h"

#include <cassert>

void test_hash_index_point_operations() {
    HashIndex index;

    assert(index.size() == 0);
    assert(!index.find(10).has_value());
    assert(!index.erase(10));

    index.insert(10, "ten");
    index.insert(20, "twenty");
    index.insert(10, "updated");

    assert(index.size() == 2);
    assert(index.find(10).value() == "updated");
    assert(index.find(20).value() == "twenty");
    assert(index.erase(10));
    assert(!index.find(10).has_value());
    assert(!index.erase(10));
}

void test_hash_index_range_query() {
    HashIndex index;

    index.insert(30, "thirty");
    index.insert(10, "ten");
    index.insert(20, "twenty");
    index.insert(40, "forty");

    const auto entries = index.range(15, 35);
    assert(entries.size() == 2);
    assert(entries[0].first == 20);
    assert(entries[0].second == "twenty");
    assert(entries[1].first == 30);
    assert(entries[1].second == "thirty");
    assert(index.range(35, 15).empty());
}

void test_storage_engine_api() {
    StorageEngine storage;

    storage.put(1, "Akhil");
    storage.put(2, "AdaptiveCore");
    assert(storage.get(1).value() == "Akhil");
    assert(storage.get(2).value() == "AdaptiveCore");
    assert(!storage.get(3).has_value());
    assert(storage.remove(1));
    assert(!storage.get(1).has_value());
    assert(!storage.remove(1));
}

int main() {
    test_hash_index_point_operations();
    test_hash_index_range_query();
    test_storage_engine_api();
    return 0;
}