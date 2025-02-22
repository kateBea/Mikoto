//
// Created by kate on 12/17/24.
//

#ifndef GENTREE_HH
#define GENTREE_HH

#include <memory>
#include <vector>
#include <functional>
#include <utility>

namespace Mikoto {

    template<typename T>
    class GenTree {
    public:
        using value_type = T;
        using reference_type = T;

    public:
        struct Node {
            template<typename... Args>
            explicit Node(Args&&... args)
                :   data{ std::forward<Args>(args)... }
            {

            }

            Node(const Node&) = default;
            Node(Node&&) = default;

            value_type data;
            std::vector<std::unique_ptr<Node>> children;

            auto IsLeaf() const -> bool {
                return children.empty();
            }
        };

        using tree_t = std::vector<std::unique_ptr<Node>>;

    public:

        template<typename... Args>
        auto Insert(Args&&... args) -> bool {
            auto result{ std::make_unique<Node>(std::forward<Args>(args)...) };

            if (result) {
                m_Nodes.emplace_back(std::move(result));
                return true;
            }

            return false;
        }

        template<typename UnaryPred, typename InputIt>
        auto InsertMultiple(UnaryPred&& pred, InputIt start, InputIt end) -> bool {
            size_t index{ 0 };

            std::unique_ptr<Node>* parentPtr{ nullptr };

            while (!parentPtr && index < m_Nodes.size()) {
                parentPtr = Find(pred, m_Nodes[index++]);
            }

            if (parentPtr) {
                for ( ; start != end; ++start) {
                    auto result{ std::make_unique<Node>(*start) };
                    (*parentPtr)->children.emplace_back(std::move(result));
                }

                return true;
            }

            return false;
        }

        template<typename UnaryPred, typename... Args>
        auto InsertChild(UnaryPred&& pred, Args&&... args) -> bool {
            size_t index{ 0 };

            std::unique_ptr<Node>* parentPtr{ nullptr };

            while (!parentPtr && index < m_Nodes.size()) {
                parentPtr = Find(pred, m_Nodes[index++]);
            }

            if (parentPtr) {
                auto result{ std::make_unique<Node>(std::forward<Args>(args)...) };
                (*parentPtr)->children.emplace_back(std::move(result));
                return true;
            }

            return false;
        }

        template<typename UnaryPred>
        auto Contains(UnaryPred&& pred) -> bool {
            auto result {
                std::ranges::find_if(
                    m_Nodes,
                    [pred](const auto& nodePtr) -> bool {
                        return Find(nodePtr, pred) != nullptr;
                    }
                )
            };

            return result != m_Nodes.end();
        }

        template<typename UnaryPred>
        auto Get(UnaryPred&& pred) -> reference_type {
            auto found{ false };
            size_t index{ 0 };

            std::unique_ptr<Node>* parentPtr{ nullptr };

            while (!parentPtr && index < m_Nodes.size()) {
                parentPtr = Find(pred, m_Nodes[index++]);
            }

            return (*parentPtr)->data;
        }

        auto GetNodes() -> tree_t& {
            return m_Nodes;
        }

        template<typename UnaryFunc>
        auto ForAll(UnaryFunc&& func) -> void {

            for (auto& node : m_Nodes) {
                Traverse(func, *node);
            }
        }

        template<typename UnaryFunc, typename UnaryPred>
        auto ForAll(UnaryFunc&& func, UnaryPred&& pred) -> void {
            for (auto& node : m_Nodes) {
                TraverseWithPred(func, pred, *node);
            }
        }

        template<typename UnaryFunc, typename UnaryPred>
        auto ForAllChildren(UnaryFunc&& func, UnaryPred&& pred) -> void {
            size_t index{ 0 };

            std::unique_ptr<Node>* parentPtr{ nullptr };

            while (!parentPtr && index < m_Nodes.size()) {
                parentPtr = Find(pred, m_Nodes[index++]);
            }

            if (parentPtr) {
                for (auto& node : (*parentPtr)->children) {
                    TraverseWithPred(func, [](const auto&) { return true; }, *node);
                }
            }
        }

        template<typename UnaryPred>
        auto Erase(UnaryPred&& pred) -> bool {
            auto erased{ false };
            size_t index{ 0 };

            while (!erased && index < m_Nodes.size()) {
                if (pred(m_Nodes[index]->data)) {
                    m_Nodes[index]->children.clear();
                    m_Nodes.erase(m_Nodes.begin() + index);
                    erased = true;
                } else {
                    erased = Erase(m_Nodes[index], pred);
                }

                ++index;
            }

            return erased;
        }

        auto Clear() -> void {
            m_Nodes.clear();
        }

    private:
        template<typename UnaryPred>
        auto Find(UnaryPred&& func, std::unique_ptr<Node>& node) -> std::unique_ptr<Node>* {
            if (func(node->data)) {
                return std::addressof(node);
            }

            for (auto& child : node->children) {
                auto result{ Find(func, child) };

                if (result) {
                    return result;
                }
            }

            return nullptr;
        }

        template<typename UnaryFunc>
        auto Traverse(UnaryFunc&& func, Node& node) -> void {
            func(node.data);

            for (auto& child : node.children) {
                Traverse(func, *child);
            }
        }

        // The very first call must not be a node that can be deleted i.e meets the predicate
        template<typename UnaryPred>
        auto Erase(std::unique_ptr<Node>& node, UnaryPred&& pred) -> bool {
            if (pred(node->data)) {
                // If the parent is the actual target erase first all of its children then
                // erase the children, return true in that case

                node->children.clear();
                node = nullptr;
                return true;
            } else {
                auto index{ 0 };
                auto found{ false };

                while (!found && index < node->children.size()) {
                    found = Erase(node->children[index], pred);

                    if (!found) {
                        ++index;
                    }
                }

                if (found) {
                    node->children.erase(node->children.begin() + index);
                }

                return found;
            }
        }

        template<typename UnaryFunc, typename UnaryPred>
        auto TraverseWithPred(UnaryFunc&& func, UnaryPred&& pred, Node& node) -> void {
            if (pred(node.data)) {
                func(node.data);
            }

            for (auto& child : node.children) {
                TraverseWithPred(func, pred, *child);
            }
        }


    private:
        tree_t m_Nodes{};
    };
}
#endif //GENTREE_HH
