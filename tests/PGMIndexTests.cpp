#include "../src/index/PGMIndex.h"

#include <cassert>

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

int main() {
    test_pgm_index_construction();
    test_piecewise_linear_segment_structure();
    return 0;
}
