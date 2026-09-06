#include "../src/adaptive/AdaptiveIndex.h"

#include <cassert>
#include <string>

void test_workload_analyzer() {
    WorkloadAnalyzer analyzer(3);
    analyzer.record(OperationType::Insert);
    analyzer.record(OperationType::PointLookup);
    assert(!analyzer.window_complete());
    analyzer.record(OperationType::RangeQuery);

    const auto snapshot = analyzer.snapshot();
    assert(analyzer.window_complete());
    assert(snapshot.inserts == 1);
    assert(snapshot.point_lookups == 1);
    assert(snapshot.range_queries == 1);
    assert(snapshot.deletes == 0);
    assert(snapshot.total_operations == 3);

    analyzer.reset_window();
    assert(analyzer.snapshot().total_operations == 0);
}

void test_policy() {
    AdaptivePolicy policy;
    assert(policy.choose({0, 1, 3, 0, 4}) == IndexChoice::BPlusTree);
    assert(policy.choose({0, 4, 0, 0, 4}) == IndexChoice::PGM);
    assert(policy.choose({2, 1, 0, 1, 4}) == IndexChoice::Hash);
}

void test_adaptive_index_operations() {
    AdaptiveIndex index(4, 0);
    index.insert(20, "twenty");
    index.insert(10, "ten");
    index.insert(30, "thirty");
    index.insert(20, "updated");

    assert(index.size() == 3);
    assert(index.find(20).value() == "updated");
    assert(index.range(10, 30).size() == 3);
    assert(index.erase(10));
    assert(!index.erase(10));
    assert(!index.find(10).has_value());
    assert(index.size() == 2);
}

void test_hash_to_pgm_switch() {
    AdaptiveIndex index(4, 0);
    for (int key = 0; key < 4; ++key) {
        index.find(key);
    }
    assert(index.current_choice() == IndexChoice::PGM);
    assert(index.find(100).has_value() == false);
    assert(index.size() == 0);
}

void test_pgm_to_bplus_to_hash_switches() {
    AdaptiveIndex index(4, 0);
    for (int key = 0; key < 4; ++key) {
        index.insert(key, std::to_string(key));
    }
    assert(index.current_choice() == IndexChoice::Hash);

    for (int operation = 0; operation < 4; ++operation) {
        index.find(operation);
    }
    assert(index.current_choice() == IndexChoice::PGM);

    for (int operation = 0; operation < 4; ++operation) {
        index.range(0, 3);
    }
    assert(index.current_choice() == IndexChoice::BPlusTree);
    assert(index.find(2).value() == "2");

    for (int operation = 0; operation < 4; ++operation) {
        index.insert(100 + operation, "extra");
    }
    assert(index.current_choice() == IndexChoice::Hash);
    assert(index.find(2).value() == "2");
    assert(index.size() == 8);
}

void test_cooldown() {
    AdaptiveIndex index(2, 2);
    index.find(1);
    index.find(2);
    assert(index.current_choice() == IndexChoice::Hash);
    index.find(3);
    index.find(4);
    assert(index.current_choice() == IndexChoice::PGM);
}

int main() {
    test_workload_analyzer();
    test_policy();
    test_adaptive_index_operations();
    test_hash_to_pgm_switch();
    test_pgm_to_bplus_to_hash_switches();
    test_cooldown();
    return 0;
}
