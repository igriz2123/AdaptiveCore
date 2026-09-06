#include "AdaptivePolicy.h"

IndexChoice AdaptivePolicy::choose(const WorkloadSnapshot& snapshot) const {
    const auto writes = snapshot.inserts + snapshot.deletes;

    if (snapshot.range_queries >= snapshot.point_lookups &&
        snapshot.range_queries >= writes) {
        return IndexChoice::BPlusTree;
    }

    if (writes == 0 && snapshot.point_lookups > 0) {
        return IndexChoice::PGM;
    }

    return IndexChoice::Hash;
}

const char* to_string(IndexChoice choice) {
    switch (choice) {
    case IndexChoice::Hash:
        return "Hash";
    case IndexChoice::BPlusTree:
        return "BPlusTree";
    case IndexChoice::PGM:
        return "PGM";
    }
    return "Unknown";
}
