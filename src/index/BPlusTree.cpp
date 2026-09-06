#include "BPlusTree.h"

#include <algorithm>
#include <stdexcept>

BPlusTree::BPlusTree(std::size_t max_keys_per_node)
    : max_keys_per_node_(max_keys_per_node == 0 ? 1 : max_keys_per_node),
      size_(0),
      root_(std::make_unique<LeafNode>()) {}

void BPlusTree::insert(int key, const std::string& value) {
    Node* current = root_.get();

    while (!current->is_leaf) {
        auto* internal = static_cast<InternalNode*>(current);
        const auto child_position =
            std::upper_bound(internal->keys.begin(), internal->keys.end(), key) -
            internal->keys.begin();
        current = internal->children[child_position].get();
    }

    auto* leaf = static_cast<LeafNode*>(current);
    const auto position = std::lower_bound(
        leaf->entries.begin(), leaf->entries.end(), key,
        [](const Entry& item, int searched_key) {
            return item.first < searched_key;
        });

    if (position != leaf->entries.end() && position->first == key) {
        position->second = value;
        return;
    }

    if (leaf->entries.size() < max_keys_per_node_) {
        leaf->entries.insert(position, {key, value});
        ++size_;
        return;
    }

    auto* parent = leaf->parent;
    if (parent != nullptr && parent->keys.size() >= max_keys_per_node_) {
        throw std::logic_error(
            "BPlusTree parent is full; internal splitting is not implemented");
    }

    std::vector<Entry> combined_entries = leaf->entries;
    combined_entries.insert(
        combined_entries.begin() + (position - leaf->entries.begin()),
        {key, value});

    const auto left_entry_count = (combined_entries.size() + 1) / 2;
    auto new_leaf = std::make_unique<LeafNode>();
    new_leaf->entries.assign(combined_entries.begin() + left_entry_count,
                             combined_entries.end());
    leaf->entries.assign(combined_entries.begin(),
                         combined_entries.begin() + left_entry_count);

    new_leaf->next_leaf = leaf->next_leaf;
    leaf->next_leaf = new_leaf.get();
    const int separator = new_leaf->entries.front().first;

    if (parent == nullptr) {
        auto old_root = std::move(root_);
        auto new_root = std::make_unique<InternalNode>();
        auto* new_root_pointer = new_root.get();

        old_root->parent = new_root_pointer;
        new_leaf->parent = new_root_pointer;
        new_root->keys.push_back(separator);
        new_root->children.push_back(std::move(old_root));
        new_root->children.push_back(std::move(new_leaf));
        root_ = std::move(new_root);
    } else {
        const auto child_position = std::find_if(
            parent->children.begin(), parent->children.end(),
            [leaf](const std::unique_ptr<Node>& child) {
                return child.get() == leaf;
            });
        const auto index = child_position - parent->children.begin();

        new_leaf->parent = parent;
        parent->keys.insert(parent->keys.begin() + index, separator);
        parent->children.insert(parent->children.begin() + index + 1,
                                std::move(new_leaf));
    }

    ++size_;
}

std::optional<std::string> BPlusTree::find(int key) const {
    const Node* current = root_.get();

    while (!current->is_leaf) {
        const auto* internal = static_cast<const InternalNode*>(current);
        const auto child_position =
            std::upper_bound(internal->keys.begin(), internal->keys.end(), key) -
            internal->keys.begin();
        current = internal->children[child_position].get();
    }

    const auto* leaf = static_cast<const LeafNode*>(current);
    const auto entry = std::lower_bound(
        leaf->entries.begin(), leaf->entries.end(), key,
        [](const Entry& item, int searched_key) {
            return item.first < searched_key;
        });

    if (entry == leaf->entries.end() || entry->first != key) {
        return std::nullopt;
    }

    return entry->second;
}

bool BPlusTree::erase(int) {
    throw std::logic_error("BPlusTree deletion is not implemented yet");
}

std::vector<Index::Entry> BPlusTree::range(int, int) const {
    throw std::logic_error("BPlusTree range queries are not implemented yet");
}

std::size_t BPlusTree::size() const {
    return size_;
}

std::size_t BPlusTree::max_keys_per_node() const {
    return max_keys_per_node_;
}

void BPlusTree::initialize_for_testing(
    const std::vector<int>& separators,
    const std::vector<std::vector<Entry>>& leaf_entries) {
    if (leaf_entries.empty() || leaf_entries.size() != separators.size() + 1) {
        throw std::invalid_argument(
            "A test tree needs one more leaf than separator keys");
    }

    if (separators.empty()) {
        auto leaf = std::make_unique<LeafNode>();
        leaf->entries = leaf_entries.front();
        size_ = leaf->entries.size();
        root_ = std::move(leaf);
        return;
    }

    auto internal = std::make_unique<InternalNode>();
    internal->keys = separators;

    for (const auto& entries : leaf_entries) {
        auto leaf = std::make_unique<LeafNode>();
        leaf->entries = entries;
        leaf->parent = internal.get();
        internal->children.push_back(std::move(leaf));
    }

    size_ = 0;
    for (const auto& entries : leaf_entries) {
        size_ += entries.size();
    }

    for (std::size_t index = 1; index < internal->children.size(); ++index) {
        auto* previous_leaf =
            static_cast<LeafNode*>(internal->children[index - 1].get());
        previous_leaf->next_leaf =
            static_cast<LeafNode*>(internal->children[index].get());
    }

    root_ = std::move(internal);
}

std::vector<std::vector<Index::Entry>> BPlusTree::leaf_chain_for_testing() const {
    const Node* current = root_.get();

    while (!current->is_leaf) {
        const auto* internal = static_cast<const InternalNode*>(current);
        current = internal->children.front().get();
    }

    std::vector<std::vector<Entry>> leaves;
    const auto* leaf = static_cast<const LeafNode*>(current);
    while (leaf != nullptr) {
        leaves.push_back(leaf->entries);
        leaf = leaf->next_leaf;
    }

    return leaves;
}