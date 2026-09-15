#include "../src/adaptive/AdaptiveIndex.h"

#include <cassert>
#include <cmath>
#include <string>

void test_workload_snapshot_features() {
    WorkloadAnalyzer analyzer(5);
    const auto empty = analyzer.snapshot();
    assert(empty.total_operations == 0);
    assert(empty.insert_ratio == 0.0);
    assert(empty.write_ratio == 0.0);
    assert(empty.point_lookup_ratio == 0.0);
    assert(empty.range_query_ratio == 0.0);
    assert(empty.delete_ratio == 0.0);

    analyzer.record(OperationType::Insert);
    analyzer.record(OperationType::PointLookup);
    analyzer.record(OperationType::RangeQuery);
    analyzer.record(OperationType::Delete);
    analyzer.record(OperationType::PointLookup);
    const auto snapshot = analyzer.snapshot();
    assert(snapshot.total_operations == 5);
    assert(std::abs(snapshot.insert_ratio - 0.2) < 1e-12);
    assert(std::abs(snapshot.write_ratio - 0.4) < 1e-12);
    assert(std::abs(snapshot.point_lookup_ratio - 0.4) < 1e-12);
    assert(std::abs(snapshot.range_query_ratio - 0.2) < 1e-12);
    assert(std::abs(snapshot.delete_ratio - 0.2) < 1e-12);
}

void test_key_distribution_features() {
    WorkloadAnalyzer uniform_like(4);
    uniform_like.record(OperationType::PointLookup, 10);
    uniform_like.record(OperationType::PointLookup, 40);
    uniform_like.record(OperationType::PointLookup, 20);
    uniform_like.record(OperationType::PointLookup, 30);
    const auto uniform_snapshot = uniform_like.snapshot();
    assert(uniform_snapshot.observed_key_count == 4);
    assert(uniform_snapshot.distinct_key_count == 4);
    assert(uniform_snapshot.minimum_key == 10);
    assert(uniform_snapshot.maximum_key == 40);
    assert(uniform_snapshot.key_span == 30);
    assert(std::abs(uniform_snapshot.key_mean - 25.0) < 1e-12);
    assert(std::abs(uniform_snapshot.key_variance - 125.0) < 1e-12);
    assert(uniform_snapshot.key_monotonicity < 1.0);
    assert(std::abs(uniform_snapshot.key_concentration - 0.25) < 1e-12);

    WorkloadAnalyzer hot_keys(4);
    for (int operation = 0; operation < 4; ++operation) {
        hot_keys.record(OperationType::PointLookup, 7);
    }
    const auto hot_snapshot = hot_keys.snapshot();
    assert(hot_snapshot.observed_key_count == 4);
    assert(hot_snapshot.distinct_key_count == 1);
    assert(hot_snapshot.key_variance == 0.0);
    assert(hot_snapshot.key_concentration == 1.0);
    assert(hot_snapshot.key_concentration > uniform_snapshot.key_concentration);

    WorkloadAnalyzer sequential(4);
    for (int key = 1; key <= 4; ++key) {
        sequential.record(OperationType::PointLookup, key);
    }
    assert(sequential.snapshot().key_monotonicity == 1.0);

    WorkloadAnalyzer range_analyzer(1);
    range_analyzer.record_range(5, 9);
    assert(range_analyzer.snapshot().observed_key_count == 2);
    assert(range_analyzer.snapshot().minimum_key == 5);
    assert(range_analyzer.snapshot().maximum_key == 9);
}

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

    WorkloadSnapshot equivalent_snapshot;
    equivalent_snapshot.point_lookups = 1;
    equivalent_snapshot.range_queries = 3;
    equivalent_snapshot.total_operations = 4;
    equivalent_snapshot.point_lookup_ratio = 0.25;
    equivalent_snapshot.range_query_ratio = 0.75;
    assert(policy.choose(equivalent_snapshot) == IndexChoice::BPlusTree);
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

void test_failed_deletes_are_analyzed() {
    AdaptiveIndex index(4, 0);
    for (int key = 0; key < 4; ++key) {
        assert(!index.erase(key));
    }
    const auto decisions = index.decision_events();
    assert(decisions.size() == 1);
    assert(decisions[0].workload.deletes == 4);
    assert(decisions[0].workload.total_operations == 4);
}

void test_hash_to_pgm_switch() {
    AdaptiveIndex index(4, 0);
    for (int key = 0; key < 4; ++key) {
        index.find(key);
    }
    assert(index.current_choice() == IndexChoice::PGM);
    const auto decisions = index.decision_events();
    assert(decisions.size() == 1);
    assert(decisions[0].operation_number == 4);
    assert(decisions[0].window_number == 1);
    assert(decisions[0].active_choice == IndexChoice::Hash);
    assert(decisions[0].selected_choice == IndexChoice::PGM);
    assert(decisions[0].workload.total_operations == 4);
    assert(decisions[0].workload.point_lookup_ratio == 1.0);
    assert(decisions[0].workload.observed_key_count == 4);
    assert(decisions[0].workload.distinct_key_count == 4);
    assert(decisions[0].workload.minimum_key == 0);
    assert(decisions[0].workload.maximum_key == 3);
    assert(decisions[0].switch_occurred);
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
    test_workload_snapshot_features();
    test_key_distribution_features();
    test_workload_analyzer();
    test_policy();
    test_adaptive_index_operations();
    test_failed_deletes_are_analyzed();
    test_hash_to_pgm_switch();
    test_pgm_to_bplus_to_hash_switches();
    test_cooldown();
    return 0;
}
