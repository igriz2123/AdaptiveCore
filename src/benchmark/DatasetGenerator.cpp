#include "DatasetGenerator.h"

#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_set>

std::vector<int> DatasetGenerator::generate_sequential(std::size_t size,
                                                       int first_key) {
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1ULL) {
        throw std::invalid_argument("Dataset is too large for integer keys");
    }

    std::vector<int> keys;
    keys.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
        keys.push_back(first_key + static_cast<int>(index));
    }
    return keys;
}

std::vector<int> DatasetGenerator::generate_random(std::size_t size,
                                                   std::uint32_t seed,
                                                   int minimum_key,
                                                   int maximum_key) {
    if (minimum_key > maximum_key) {
        throw std::invalid_argument("Random key range is invalid");
    }

    const auto key_count = static_cast<std::uint64_t>(maximum_key) -
                           static_cast<std::uint64_t>(minimum_key) + 1;
    if (size > key_count) {
        throw std::invalid_argument("Random key range is too small");
    }

    std::vector<int> keys;
    keys.reserve(size);
    std::mt19937 generator(seed);
    std::uniform_int_distribution<int> distribution(minimum_key, maximum_key);

    std::unordered_set<int> selected_keys;
    selected_keys.reserve(size);
    while (keys.size() < size) {
        const int key = distribution(generator);
        if (selected_keys.insert(key).second) {
            keys.push_back(key);
        }
    }

    return keys;
}

std::vector<int> DatasetGenerator::generate_clustered(
    std::size_t size,
    std::uint32_t seed,
    std::size_t cluster_count,
    int cluster_width,
    int cluster_spacing,
    int first_cluster_key) {
    if (size == 0) {
        return {};
    }
    if (cluster_count == 0 || cluster_width <= 0 || cluster_spacing < cluster_width) {
        throw std::invalid_argument("Cluster configuration is invalid");
    }

    const auto candidate_count = cluster_count *
                                 static_cast<std::size_t>(cluster_width);
    if (size > candidate_count) {
        throw std::invalid_argument("Cluster regions are too small");
    }

    std::vector<int> candidates;
    candidates.reserve(candidate_count);
    for (std::size_t cluster = 0; cluster < cluster_count; ++cluster) {
        const int cluster_start =
            first_cluster_key + static_cast<int>(cluster) * cluster_spacing;
        for (int offset = 0; offset < cluster_width; ++offset) {
            candidates.push_back(cluster_start + offset);
        }
    }

    std::mt19937 generator(seed);
    std::shuffle(candidates.begin(), candidates.end(), generator);
    candidates.resize(size);
    return candidates;
}
