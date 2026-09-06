#include "../src/benchmark/DatasetGenerator.h"

#include <cassert>
#include <unordered_set>

void test_sequential_dataset() {
    const auto keys = DatasetGenerator::generate_sequential(5, 100);

    assert(keys.size() == 5);
    assert(keys[0] == 100);
    assert(keys[1] == 101);
    assert(keys[2] == 102);
    assert(keys[3] == 103);
    assert(keys[4] == 104);
}

void test_random_dataset() {
    const auto keys = DatasetGenerator::generate_random(100, 42, 0, 10000);
    std::unordered_set<int> unique_keys(keys.begin(), keys.end());

    assert(keys.size() == 100);
    assert(unique_keys.size() == keys.size());
}

void test_clustered_dataset() {
    const auto keys = DatasetGenerator::generate_clustered(60, 42, 3, 20,
                                                           1000, 500);
    std::unordered_set<int> unique_keys(keys.begin(), keys.end());

    assert(keys.size() == 60);
    assert(unique_keys.size() == keys.size());
    for (const int key : keys) {
        const bool in_first_cluster = key >= 500 && key < 520;
        const bool in_second_cluster = key >= 1500 && key < 1520;
        const bool in_third_cluster = key >= 2500 && key < 2520;
        assert(in_first_cluster || in_second_cluster || in_third_cluster);
    }
}

void test_reproducibility() {
    const auto random_keys = DatasetGenerator::generate_random(100, 7, -5000,
                                                               5000);
    const auto same_random_keys =
        DatasetGenerator::generate_random(100, 7, -5000, 5000);
    assert(random_keys == same_random_keys);

    const auto clustered_keys = DatasetGenerator::generate_clustered(40, 7);
    const auto same_clustered_keys = DatasetGenerator::generate_clustered(40, 7);
    assert(clustered_keys == same_clustered_keys);
}

int main() {
    test_sequential_dataset();
    test_random_dataset();
    test_clustered_dataset();
    test_reproducibility();
    return 0;
}
