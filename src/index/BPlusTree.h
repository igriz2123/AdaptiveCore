#pragma once

#include "Index.h"

#include <cstddef>
#include <memory>
#include <vector>

class BPlusTree final : public Index {
public:
    explicit BPlusTree(std::size_t max_keys_per_node = 4);

    void insert(int key, const std::string& value) override;
    std::optional<std::string> find(int key) const override;
    bool erase(int key) override;
    std::vector<Entry> range(int lower_key, int upper_key) const override;
    std::size_t size() const override;

    std::size_t max_keys_per_node() const;

    void initialize_for_testing(
        const std::vector<int>& separators,
        const std::vector<std::vector<Entry>>& leaf_entries);

    std::vector<std::vector<Entry>> leaf_chain_for_testing() const;
    bool validate_structure_for_testing() const;

private:
    struct InternalNode;

    struct Node {
        explicit Node(bool leaf) : is_leaf(leaf), parent(nullptr) {}
        virtual ~Node() = default;

        bool is_leaf;
        std::vector<int> keys;
        InternalNode* parent;
    };

    struct InternalNode final : Node {
        InternalNode() : Node(false) {}

        std::vector<std::unique_ptr<Node>> children;
    };

    struct LeafNode final : Node {
        LeafNode() : Node(true), next_leaf(nullptr) {}

        std::vector<Entry> entries;
        LeafNode* next_leaf;
    };

    void add_child_to_parent(Node* left_child,
                             int separator,
                             std::unique_ptr<Node> right_child);
    void split_internal(InternalNode* node);

    std::size_t max_keys_per_node_;
    std::size_t size_;
    std::unique_ptr<Node> root_;
};