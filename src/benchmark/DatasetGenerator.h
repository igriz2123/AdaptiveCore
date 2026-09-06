#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class DatasetGenerator {
public:
    static std::vector<int> generate_sequential(std::size_t size,
                                                int first_key = 0);

    static std::vector<int> generate_random(std::size_t size,
                                            std::uint32_t seed,
                                            int minimum_key,
                                            int maximum_key);

    static std::vector<int> generate_clustered(
        std::size_t size,
        std::uint32_t seed,
        std::size_t cluster_count = 4,
        int cluster_width = 100,
        int cluster_spacing = 1000,
        int first_cluster_key = 0);
};
