#include "../src/index/BPlusTree.h"
#include "../src/index/HashIndex.h"
#include "../src/storage/StorageEngine.h"

#include <cassert>
#include <stdexcept>

void test_b_plus_tree_initial_state() {
    BPlusTree default_tree;
    BPlusTree configured_tree(8);

    assert(default_tree.size() == 0);
    assert(default_tree.max_keys_per_node() == 4);
    assert(configured_tree.size() == 0);
    assert(configured_tree.max_keys_per_node() == 8);
}

void test_b_plus_tree_search() {
    BPlusTree empty_tree;
    assert(!empty_tree.find(10).has_value());

    BPlusTree leaf_tree;
    leaf_tree.insert(20, "twenty");
    leaf_tree.insert(10, "ten");
    assert(leaf_tree.size() == 2);
    assert(leaf_tree.find(10).value() == "ten");
    assert(!leaf_tree.find(15).has_value());

    BPlusTree internal_tree;
    internal_tree.initialize_for_testing(
        {20}, {{{10, "ten"}}, {{20, "twenty"}, {30, "thirty"}}});
    assert(internal_tree.find(10).value() == "ten");
    assert(internal_tree.find(30).value() == "thirty");
    assert(!internal_tree.find(15).has_value());
    assert(!internal_tree.find(40).has_value());
}

void test_b_plus_tree_insert_updates_and_counts() {
    BPlusTree tree(4);

    tree.insert(30, "thirty");
    tree.insert(10, "ten");
    tree.insert(20, "twenty");
    assert(tree.size() == 3);
    assert(tree.find(10).value() == "ten");
    assert(tree.find(20).value() == "twenty");
    assert(tree.find(30).value() == "thirty");

    tree.insert(20, "updated");
    assert(tree.size() == 3);
    assert(tree.find(20).value() == "updated");
}

void test_b_plus_tree_root_leaf_split() {
    BPlusTree tree(3);

    tree.insert(30, "thirty");
    tree.insert(10, "ten");
    tree.insert(20, "twenty");
    tree.insert(40, "forty");

    const auto leaves = tree.leaf_chain_for_testing();
    assert(leaves.size() == 2);
    assert(leaves[0].size() == 2);
    assert(leaves[0][0].first == 10);
    assert(leaves[0][1].first == 20);
    assert(leaves[1].size() == 2);
    assert(leaves[1][0].first == 30);
    assert(leaves[1][1].first == 40);
    assert(tree.size() == 4);
    assert(tree.find(10).value() == "ten");
    assert(tree.find(20).value() == "twenty");
    assert(tree.find(30).value() == "thirty");
    assert(tree.find(40).value() == "forty");
}

void test_b_plus_tree_non_root_leaf_split() {
    BPlusTree tree(3);
    tree.initialize_for_testing({30}, {{{10, "ten"}, {20, "twenty"}, {25, "twenty-five"}},
                                      {{30, "thirty"}}});

    tree.insert(15, "fifteen");
    const auto leaves = tree.leaf_chain_for_testing();
    assert(leaves.size() == 3);
    assert(leaves[0][0].first == 10);
    assert(leaves[0][1].first == 15);
    assert(leaves[1][0].first == 20);
    assert(leaves[1][1].first == 25);
    assert(leaves[2][0].first == 30);
    assert(tree.size() == 5);
    assert(tree.find(15).value() == "fifteen");
    assert(tree.find(25).value() == "twenty-five");
}

void test_b_plus_tree_duplicate_update_does_not_split() {
    BPlusTree tree(2);
    tree.insert(10, "ten");
    tree.insert(20, "twenty");
    tree.insert(20, "updated");

    const auto leaves = tree.leaf_chain_for_testing();
    assert(leaves.size() == 1);
    assert(tree.size() == 2);
    assert(tree.find(20).value() == "updated");
}

void test_b_plus_tree_root_internal_split() {
    BPlusTree tree(2);
    tree.initialize_for_testing(
        {20, 40}, {{{10, "ten"}, {15, "fifteen"}},
                   {{20, "twenty"}, {25, "twenty-five"}},
                   {{40, "forty"}, {45, "forty-five"}}});

    tree.insert(12, "twelve");

    assert(tree.size() == 7);
    assert(tree.find(12).value() == "twelve");
    assert(tree.find(45).value() == "forty-five");
    assert(tree.validate_structure_for_testing());
}

void test_b_plus_tree_recursive_internal_splits() {
    BPlusTree tree(2);

    for (int key = 1; key <= 24; ++key) {
        tree.insert(key, std::to_string(key));
    }

    assert(tree.size() == 24);
    assert(tree.validate_structure_for_testing());

    const auto leaves = tree.leaf_chain_for_testing();
    assert(leaves.size() > 4);
    int expected_key = 1;
    for (const auto& leaf : leaves) {
        for (const auto& entry : leaf) {
            assert(entry.first == expected_key);
            assert(entry.second == std::to_string(expected_key));
            assert(tree.find(expected_key).value() == entry.second);
            ++expected_key;
        }
    }
    assert(expected_key == 25);
}

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
    test_b_plus_tree_initial_state();
    test_b_plus_tree_search();
    test_b_plus_tree_insert_updates_and_counts();
    test_b_plus_tree_root_leaf_split();
    test_b_plus_tree_non_root_leaf_split();
    test_b_plus_tree_duplicate_update_does_not_split();
    test_b_plus_tree_root_internal_split();
    test_b_plus_tree_recursive_internal_splits();
    test_hash_index_point_operations();
    test_hash_index_range_query();
    test_storage_engine_api();
    return 0;
}