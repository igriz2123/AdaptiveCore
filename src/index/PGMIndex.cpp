#include "PGMIndex.h"

#include <stdexcept>

PGMIndex::PGMIndex(std::size_t error_bound)
    : error_bound_(error_bound) {}

void PGMIndex::insert(int, const std::string&) {
    throw std::logic_error("PGMIndex insertion is not implemented yet");
}

std::optional<std::string> PGMIndex::find(int) const {
    throw std::logic_error("PGMIndex search is not implemented yet");
}

bool PGMIndex::erase(int) {
    throw std::logic_error("PGMIndex deletion is not implemented yet");
}

std::vector<Index::Entry> PGMIndex::range(int, int) const {
    throw std::logic_error("PGMIndex range queries are not implemented yet");
}

std::size_t PGMIndex::size() const {
    return entries_.size();
}

std::size_t PGMIndex::error_bound() const {
    return error_bound_;
}

const std::vector<PGMIndex::PiecewiseLinearSegment>& PGMIndex::segments() const {
    return segments_;
}
