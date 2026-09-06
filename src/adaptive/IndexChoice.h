#pragma once

enum class IndexChoice {
    Hash,
    BPlusTree,
    PGM
};

const char* to_string(IndexChoice choice);
