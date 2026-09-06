#include "../src/index/PGMIndex.h"

#include <cassert>
#include <cmath>
#include <vector>

void test_pgm_index_construction() {
    PGMIndex default_index;
    PGMIndex configured_index(8);

    assert(default_index.error_bound() == 32);
    assert(configured_index.error_bound() == 8);
    assert(default_index.size() == 0);
    assert(configured_index.size() == 0);
    assert(default_index.segments().empty());
}

void test_piecewise_linear_segment_structure() {
    const PGMIndex::PiecewiseLinearSegment segment{
        10, 100, 0.5, -5.0, 4, 20};

    assert(segment.first_key == 10);
    assert(segment.last_key == 100);
    assert(segment.slope == 0.5);
    assert(segment.intercept == -5.0);
    assert(segment.starting_position == 4);
    assert(segment.ending_position == 20);
}

void test_empty_model() {
    PGMIndex index(0);
    index.load_sorted_entries_for_testing({});

    assert(index.size() == 0);
    assert(index.segments().empty());
}

void test_single_entry_model() {
    PGMIndex index(0);
    index.load_sorted_entries_for_testing({{42, "answer"}});

    assert(index.size() == 1);
    assert(index.segments().size() == 1);
    const auto& segment = index.segments().front();
    assert(segment.first_key == 42);
    assert(segment.last_key == 42);
    assert(segment.slope == 0.0);
    assert(segment.intercept == 0.0);
    assert(segment.starting_position == 0);
    assert(segment.ending_position == 0);
}

void test_sequential_model_metadata() {
    PGMIndex index(0);
    index.load_sorted_entries_for_testing(
        {{30, "thirty"}, {10, "ten"}, {20, "twenty"}});

    assert(index.size() == 3);
    assert(index.segments().size() == 1);
    const auto& segment = index.segments().front();
    assert(segment.first_key == 10);
    assert(segment.last_key == 30);
    assert(std::abs(segment.slope - 0.1) < 1e-12);
    assert(std::abs(segment.intercept + 1.0) < 1e-12);
    assert(segment.starting_position == 0);
    assert(segment.ending_position == 2);
}

void test_non_uniform_keys_create_multiple_segments() {
    PGMIndex index(0);
    index.load_sorted_entries_for_testing(
        {{0, "zero"}, {1, "one"}, {10, "ten"}});

    assert(index.segments().size() == 2);
    assert(index.segments()[0].first_key == 0);
    assert(index.segments()[0].last_key == 1);
    assert(index.segments()[0].starting_position == 0);
    assert(index.segments()[0].ending_position == 1);
    assert(index.segments()[1].first_key == 10);
    assert(index.segments()[1].last_key == 10);
    assert(index.segments()[1].starting_position == 2);
    assert(index.segments()[1].ending_position == 2);
}

void test_predictions_stay_within_error_bound() {
    constexpr std::size_t error_bound = 1;
    PGMIndex index(error_bound);
    const std::vector<int> keys = {0, 1, 2, 100};
    index.load_sorted_entries_for_testing(
        {{0, "zero"}, {1, "one"}, {2, "two"}, {100, "one hundred"}});

    for (const auto& segment : index.segments()) {
        for (std::size_t position = segment.starting_position;
             position <= segment.ending_position; ++position) {
            const double prediction =
                segment.slope * static_cast<double>(keys[position]) +
                segment.intercept;
            assert(std::abs(prediction - static_cast<double>(position)) <=
                   static_cast<double>(error_bound));
        }
    }

    assert(index.segments().size() > 1);
}

void test_duplicate_keys_are_removed() {
    PGMIndex index(0);
    index.load_sorted_entries_for_testing(
        {{20, "old"}, {10, "ten"}, {20, "new"}});

    assert(index.size() == 2);
    assert(index.segments().size() == 1);
}

void test_empty_and_single_entry_search() {
    PGMIndex empty_index;
    assert(!empty_index.find(10).has_value());

    PGMIndex single_entry_index(0);
    single_entry_index.load_sorted_entries_for_testing({{42, "answer"}});
    assert(single_entry_index.find(42).value() == "answer");
    assert(!single_entry_index.find(41).has_value());
}

void test_boundary_and_missing_key_searches() {
    PGMIndex index(1);
    index.load_sorted_entries_for_testing(
        {{10, "ten"}, {20, "twenty"}, {35, "thirty-five"}, {50, "fifty"}});

    assert(index.find(10).value() == "ten");
    assert(index.find(50).value() == "fifty");
    assert(index.find(35).value() == "thirty-five");
    assert(!index.find(9).has_value());
    assert(!index.find(51).has_value());
    assert(!index.find(30).has_value());
}

void test_search_across_multiple_segments() {
    PGMIndex index(0);
    index.load_sorted_entries_for_testing(
        {{0, "zero"}, {1, "one"}, {2, "two"}, {100, "one hundred"},
         {101, "one hundred one"}, {102, "one hundred two"}});

    assert(index.segments().size() == 2);
    for (const auto& entry : std::vector<Index::Entry>{{0, "zero"},
                                                        {1, "one"},
                                                        {2, "two"},
                                                        {100, "one hundred"},
                                                        {101, "one hundred one"},
                                                        {102, "one hundred two"}}) {
        assert(index.find(entry.first).value() == entry.second);
    }
    assert(!index.find(50).has_value());
}

void test_bounded_error_correction() {
    PGMIndex index(1);
    index.load_sorted_entries_for_testing(
        {{0, "zero"}, {10, "ten"}, {11, "eleven"}, {12, "twelve"}});

    assert(index.find(10).value() == "ten");
    assert(index.find(11).value() == "eleven");
    assert(index.find(12).value() == "twelve");
    assert(!index.find(9).has_value());
}

void test_insert_keeps_entries_sorted_and_rebuilds_model() {
    PGMIndex index(0);

    index.insert(30, "thirty");
    assert(index.size() == 1);
    assert(index.model_rebuild_count_for_testing() == 0);
    assert(index.find(30).value() == "thirty");
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(index.segments().front().first_key == 30);

    index.insert(10, "ten");
    index.insert(20, "twenty");
    index.insert(25, "twenty-five");

    assert(index.size() == 4);
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(index.find(10).value() == "ten");
    assert(index.model_rebuild_count_for_testing() == 2);
    assert(index.find(20).value() == "twenty");
    assert(index.find(25).value() == "twenty-five");
    assert(index.find(30).value() == "thirty");
    assert(index.segments().front().first_key == 10);
    assert(index.segments().back().last_key == 30);
    assert(index.segments().front().starting_position == 0);
    assert(index.segments().back().ending_position == 3);
}

void test_insert_updates_without_increasing_size() {
    PGMIndex index(0);
    index.insert(20, "old");
    index.insert(10, "ten");
    index.insert(30, "thirty");

    assert(index.model_rebuild_count_for_testing() == 0);
    assert(index.find(20).value() == "twenty");
    assert(index.model_rebuild_count_for_testing() == 1);
    index.insert(20, "updated");

    assert(index.size() == 3);
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(index.find(20).value() == "updated");
    assert(index.find(10).value() == "ten");
    assert(index.find(30).value() == "thirty");
    assert(index.model_rebuild_count_for_testing() == 2);
}

void test_insert_rebuilds_multiple_segments() {
    PGMIndex index(0);
    index.insert(0, "zero");
    index.insert(1, "one");
    index.insert(2, "two");
    index.insert(100, "one hundred");

    assert(index.size() == 4);
    assert(index.segments().size() == 2);
    for (const auto& entry : std::vector<Index::Entry>{{0, "zero"},
                                                        {1, "one"},
                                                        {2, "two"},
                                                        {100, "one hundred"}}) {
        assert(index.find(entry.first).value() == entry.second);
    }
}

void test_erase_empty_and_missing_key() {
    PGMIndex empty_index;
    assert(!empty_index.erase(10));
    assert(empty_index.size() == 0);
    assert(empty_index.segments().empty());

    PGMIndex index(0);
    index.insert(10, "ten");
    index.insert(20, "twenty");
    assert(index.model_rebuild_count_for_testing() == 0);
    assert(!index.erase(15));
    assert(index.size() == 2);
    assert(index.model_rebuild_count_for_testing() == 0);
    assert(index.find(10).value() == "ten");
    assert(index.find(20).value() == "twenty");
}

void test_erase_boundaries_and_middle() {
    PGMIndex index(0);
    index.insert(10, "ten");
    index.insert(20, "twenty");
    index.insert(30, "thirty");
    index.insert(40, "forty");

    assert(index.model_rebuild_count_for_testing() == 0);
    assert(index.erase(10));
    assert(index.size() == 3);
    assert(index.model_rebuild_count_for_testing() == 0);
    assert(!index.find(10).has_value());
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(index.find(20).value() == "twenty");

    assert(index.erase(30));
    assert(index.size() == 2);
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(!index.find(30).has_value());
    assert(index.model_rebuild_count_for_testing() == 2);
    assert(index.find(20).value() == "twenty");
    assert(index.find(40).value() == "forty");

    assert(index.erase(40));
    assert(index.size() == 1);
    assert(!index.find(40).has_value());
    assert(index.find(20).value() == "twenty");
}

void test_erase_multiple_segments_and_all_keys() {
    PGMIndex index(0);
    for (const auto& entry : std::vector<Index::Entry>{{0, "zero"},
                                                        {1, "one"},
                                                        {2, "two"},
                                                        {100, "one hundred"},
                                                        {101, "one hundred one"},
                                                        {102, "one hundred two"}}) {
        index.insert(entry.first, entry.second);
    }

    assert(index.model_rebuild_count_for_testing() == 0);
    assert(index.erase(1));
    assert(index.erase(100));
    assert(index.size() == 4);
    assert(index.model_rebuild_count_for_testing() == 0);
    assert(!index.find(1).has_value());
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(!index.find(100).has_value());
    assert(index.find(0).value() == "zero");
    assert(index.find(102).value() == "one hundred two");
    assert(index.segments().front().starting_position == 0);
    assert(index.segments().back().ending_position == index.size() - 1);

    assert(index.erase(0));
    assert(index.erase(2));
    assert(index.erase(101));
    assert(index.erase(102));
    assert(index.size() == 0);
    assert(index.segments().empty());
    assert(!index.find(101).has_value());
}

void test_range_queries() {
    PGMIndex index(0);
    index.insert(10, "ten");
    index.insert(20, "twenty");
    index.insert(30, "thirty");
    index.insert(40, "forty");

    const std::vector<Index::Entry> middle_entries = {
        {20, "twenty"}, {30, "thirty"}};
    const std::vector<Index::Entry> single_entry = {{30, "thirty"}};
    const std::vector<Index::Entry> all_entries = {
        {10, "ten"}, {20, "twenty"}, {30, "thirty"}, {40, "forty"}};

    assert(index.range(20, 30) == middle_entries);
    assert(index.range(21, 39) == single_entry);
    assert(index.range(15, 15).empty());
    assert(index.range(35, 25).empty());
    assert(index.range(10, 40) == all_entries);
}

void test_range_after_insert_and_erase() {
    PGMIndex index(0);
    index.insert(100, "one hundred");
    index.insert(0, "zero");
    index.insert(1, "one");
    index.insert(2, "two");
    index.insert(101, "one hundred one");
    index.insert(102, "one hundred two");

    assert(index.segments().size() == 2);
    const auto before_erase = index.range(1, 101);
    assert(before_erase.size() == 4);
    assert(before_erase[0].first == 1);
    assert(before_erase[1].first == 2);
    assert(before_erase[2].first == 100);
    assert(before_erase[3].first == 101);

    assert(index.erase(100));
    const auto after_erase = index.range(1, 101);
    assert(after_erase.size() == 3);
    assert(after_erase[0].first == 1);
    assert(after_erase[1].first == 2);
    assert(after_erase[2].first == 101);
}

void test_bulk_load_empty() {
    PGMIndex index;
    index.insert(10, "ten");
    const auto rebuilds_before = index.model_rebuild_count_for_testing();

    index.bulk_load({});

    assert(index.size() == 0);
    assert(index.segments().empty());
    assert(index.model_rebuild_count_for_testing() == rebuilds_before + 1);
}

void test_bulk_load_sorted_and_random_input() {
    PGMIndex sorted_index(0);
    sorted_index.bulk_load({{10, "ten"}, {20, "twenty"}, {30, "thirty"}});
    assert(sorted_index.size() == 3);
    assert(sorted_index.find(10).value() == "ten");
    assert(sorted_index.find(20).value() == "twenty");
    assert(sorted_index.find(30).value() == "thirty");

    PGMIndex random_index(0);
    random_index.bulk_load({{30, "thirty"}, {10, "ten"}, {20, "twenty"}});
    assert(random_index.size() == 3);
    const auto sorted_entries = std::vector<Index::Entry>{
        {10, "ten"}, {20, "twenty"}, {30, "thirty"}};
    assert(random_index.range(10, 30) == sorted_entries);
}

void test_bulk_load_duplicate_resolution_matches_insert() {
    PGMIndex bulk_index(0);
    bulk_index.bulk_load({{20, "first"}, {10, "ten"}, {20, "second"},
                          {20, "last"}});

    PGMIndex insert_index(0);
    insert_index.insert(20, "first");
    insert_index.insert(10, "ten");
    insert_index.insert(20, "second");
    insert_index.insert(20, "last");

    assert(bulk_index.size() == 2);
    assert(bulk_index.find(20).value() == "last");
    assert(bulk_index.find(10).value() == "ten");
    assert(bulk_index.range(10, 20) == insert_index.range(10, 20));
}

void test_bulk_load_rebuilds_once_and_supports_mutations() {
    PGMIndex index(0);
    const std::vector<Index::Entry> entries = {
        {0, "zero"}, {1, "one"}, {2, "two"}, {100, "one hundred"},
        {101, "one hundred one"}, {102, "one hundred two"}};

    index.bulk_load(entries);
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(index.segments().size() == 2);
    assert(index.size() == entries.size());
    assert(index.find(101).value() == "one hundred one");
    assert(index.model_rebuild_count_for_testing() == 1);

    const auto expected_range = std::vector<Index::Entry>{
        {1, "one"}, {2, "two"}, {100, "one hundred"}};
    assert(index.range(1, 100) == expected_range);

    assert(index.erase(1));
    index.insert(50, "fifty");
    assert(!index.find(1).has_value());
    assert(index.find(50).value() == "fifty");
    assert(index.size() == entries.size());
}

void test_deferred_rebuild_preserves_all_values() {
    PGMIndex index(0);
    for (int key = 0; key < 10; ++key) {
        index.insert(key, "value-" + std::to_string(key));
    }

    assert(index.model_rebuild_count_for_testing() == 0);
    for (int key = 0; key < 10; ++key) {
        assert(index.find(key).value() == "value-" + std::to_string(key));
    }
    assert(index.model_rebuild_count_for_testing() == 1);

    for (int key = 0; key < 5; ++key) {
        assert(index.erase(key));
    }
    assert(index.model_rebuild_count_for_testing() == 1);
    assert(index.size() == 5);
    assert(!index.find(0).has_value());
    assert(index.model_rebuild_count_for_testing() == 2);
    for (int key = 5; key < 10; ++key) {
        assert(index.find(key).value() == "value-" + std::to_string(key));
    }
}

int main() {
    test_pgm_index_construction();
    test_piecewise_linear_segment_structure();
    test_empty_model();
    test_single_entry_model();
    test_sequential_model_metadata();
    test_non_uniform_keys_create_multiple_segments();
    test_predictions_stay_within_error_bound();
    test_duplicate_keys_are_removed();
    test_empty_and_single_entry_search();
    test_boundary_and_missing_key_searches();
    test_search_across_multiple_segments();
    test_bounded_error_correction();
    test_insert_keeps_entries_sorted_and_rebuilds_model();
    test_insert_updates_without_increasing_size();
    test_insert_rebuilds_multiple_segments();
    test_erase_empty_and_missing_key();
    test_erase_boundaries_and_middle();
    test_erase_multiple_segments_and_all_keys();
    test_range_queries();
    test_range_after_insert_and_erase();
    test_bulk_load_empty();
    test_bulk_load_sorted_and_random_input();
    test_bulk_load_duplicate_resolution_matches_insert();
    test_bulk_load_rebuilds_once_and_supports_mutations();
    test_deferred_rebuild_preserves_all_values();
    return 0;
}
