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
    assert(index.segments().size() == 1);
    assert(index.segments().front().first_key == 30);

    index.insert(10, "ten");
    index.insert(20, "twenty");
    index.insert(25, "twenty-five");

    assert(index.size() == 4);
    assert(index.find(10).value() == "ten");
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

    const auto segment_count_before_update = index.segments().size();
    index.insert(20, "updated");

    assert(index.size() == 3);
    assert(index.find(20).value() == "updated");
    assert(index.find(10).value() == "ten");
    assert(index.find(30).value() == "thirty");
    assert(index.segments().size() == segment_count_before_update);
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
    return 0;
}
