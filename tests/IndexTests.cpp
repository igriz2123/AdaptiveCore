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

void test_b_plus_tree_range_queries() {
    BPlusTree single_leaf_tree(4);
    single_leaf_tree.insert(30, "thirty");
    single_leaf_tree.insert(10, "ten");
    single_leaf_tree.insert(20, "twenty");

    const auto inside_one_leaf = single_leaf_tree.range(15, 25);
    assert(inside_one_leaf.size() == 1);
    assert(inside_one_leaf[0].first == 20);

    BPlusTree multi_leaf_tree(3);
    multi_leaf_tree.insert(40, "forty");
    multi_leaf_tree.insert(10, "ten");
    multi_leaf_tree.insert(30, "thirty");
    multi_leaf_tree.insert(20, "twenty");
    multi_leaf_tree.insert(50, "fifty");

    const auto spanning_leaves = multi_leaf_tree.range(15, 45);
    assert(spanning_leaves.size() == 3);
    assert(spanning_leaves[0].first == 20);
    assert(spanning_leaves[1].first == 30);
    assert(spanning_leaves[2].first == 40);
    assert(spanning_leaves[0].first < spanning_leaves[1].first);
    assert(spanning_leaves[1].first < spanning_leaves[2].first);

    const auto exact_boundaries = multi_leaf_tree.range(20, 40);
    assert(exact_boundaries.size() == 3);
    assert(exact_boundaries.front().first == 20);
    assert(exact_boundaries.back().first == 40);

    const auto missing_boundaries = multi_leaf_tree.range(21, 39);
    assert(missing_boundaries.size() == 1);
    assert(missing_boundaries[0].first == 30);

    assert(multi_leaf_tree.range(100, 200).empty());
    assert(multi_leaf_tree.range(45, 15).empty());
}

void test_b_plus_tree_range_across_internal_levels() {
    BPlusTree tree(2);
    for (int key = 1; key <= 24; ++key) {
        tree.insert(key, std::to_string(key));
    }

    const auto results = tree.range(5, 20);
    assert(results.size() == 16);
    for (std::size_t index = 0; index < results.size(); ++index) {
        const int expected_key = static_cast<int>(index) + 5;
        assert(results[index].first == expected_key);
        assert(results[index].second == std::to_string(expected_key));
    }
}

void test_b_plus_tree_erase() {
    BPlusTree single_leaf_tree(4);
    single_leaf_tree.insert(10, "ten");
    single_leaf_tree.insert(20, "twenty");
    single_leaf_tree.insert(30, "thirty");

    assert(single_leaf_tree.erase(20));
    assert(single_leaf_tree.size() == 2);
    assert(!single_leaf_tree.find(20).has_value());
    const auto single_leaf_range = single_leaf_tree.range(10, 30);
    assert(single_leaf_range.size() == 2);
    assert(single_leaf_range[0].first == 10);
    assert(single_leaf_range[1].first == 30);

    assert(!single_leaf_tree.erase(20));
    assert(!single_leaf_tree.erase(25));
    assert(single_leaf_tree.size() == 2);

    BPlusTree multi_level_tree(2);
    for (int key = 1; key <= 24; ++key) {
        multi_level_tree.insert(key, std::to_string(key));
    }

    assert(multi_level_tree.erase(1));
    assert(multi_level_tree.erase(12));
    assert(multi_level_tree.erase(24));
    assert(multi_level_tree.size() == 21);
    assert(!multi_level_tree.find(1).has_value());
    assert(!multi_level_tree.find(12).has_value());
    assert(!multi_level_tree.find(24).has_value());
    assert(multi_level_tree.find(11).value() == "11");
    assert(multi_level_tree.find(13).value() == "13");

    const auto multi_level_range = multi_level_tree.range(1, 24);
    assert(multi_level_range.size() == 21);
    for (std::size_t index = 1; index < multi_level_range.size(); ++index) {
        assert(multi_level_range[index - 1].first <
               multi_level_range[index].first);
    }

    assert(multi_level_tree.erase(2));
    assert(multi_level_tree.erase(3));
    assert(multi_level_tree.erase(4));
    assert(multi_level_tree.size() == 18);
    assert(!multi_level_tree.erase(2));
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
    test_b_plus_tree_range_queries();
    test_b_plus_tree_range_across_internal_levels();
    test_b_plus_tree_erase();
    test_hash_index_point_operations();
    test_hash_index_range_query();
    test_storage_engine_api();
    return 0;
}