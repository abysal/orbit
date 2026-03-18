#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef ORB_DUMP_MERMAID
#include <format>
#include <functional>
#include <sstream>
#include <string>
#endif

namespace orb {

    template <typename KeyVal>
    class GraphBuilder;

    template <typename KeyVal>
    class Graph {
    public:
        struct Node {
            KeyVal value{};
            std::vector<Node*> children{};
        };

#ifdef ORB_DUMP_MERMAID
        [[nodiscard]] std::string as_mermaid() const {
            std::stringstream ss{};
            ss << "graph LR\n";

            std::unordered_set<const Node*> visited;

            std::move_only_function<void(const Node*)> dfs = [&](const Node* node) {
                if (!node || visited.contains(node)) return;
                visited.insert(node);

                for (auto* child : node->children) {
                    ss << std::format(
                        "{} --> {}\n",
                        static_cast<size_t>(node->value),
                        static_cast<size_t>(child->value)
                    );
                    dfs(child);
                }
            };

            for (auto* root : m_roots) {
                dfs(root);
            }

            return ss.str();
        }
#endif

    private:
        friend class GraphBuilder<KeyVal>;

        std::vector<Node*> m_roots{};                 // top-level nodes
        std::vector<std::unique_ptr<Node>> m_graph{}; // owns all nodes
    };

    template <typename KeyVal>
    class GraphBuilder {
    public:
        GraphBuilder& add_root(KeyVal&& value) {
            m_graph_data[std::move(value)] = {};
            return *this;
        }

        GraphBuilder& add_child(const KeyVal& parent, KeyVal&& child) {
            m_graph_data[parent].emplace_back(std::move(child));
            return *this;
        }

        Graph<KeyVal> as_graph() {
            using G = Graph<KeyVal>;
            G graph{};

            std::unordered_map<KeyVal, typename G::Node*> node_ptrs;
            std::unordered_set<KeyVal> children_set;

            for (const auto& [key, children] : m_graph_data) {
                if (!node_ptrs.contains(key)) {
                    auto node = std::make_unique<typename G::Node>();
                    node->value = key;
                    node_ptrs[key] = node.get();
                    graph.m_graph.push_back(std::move(node));
                }

                for (const auto& child_val : children) {
                    if (!node_ptrs.contains(child_val)) {
                        auto child_node = std::make_unique<typename G::Node>();
                        child_node->value = child_val;
                        node_ptrs[child_val] = child_node.get();
                        graph.m_graph.push_back(std::move(child_node));
                    }
                    children_set.insert(child_val);
                }
            }

            for (const auto& [key, children] : m_graph_data) {
                auto* parent_node = node_ptrs[key];
                for (const auto& child_val : children) {
                    parent_node->children.push_back(node_ptrs[child_val]);
                }
            }

            for (const auto& [key, node] : node_ptrs) {
                if (!children_set.contains(key)) {
                    graph.m_roots.push_back(node);
                }
            }

            return graph;
        }

#ifdef ORB_DUMP_MERMAID
        [[nodiscard]] std::string as_mermaid() const {
            std::stringstream ss{};
            ss << "graph LR\n";

            for (const auto& [key, values] : m_graph_data) {
                for (const auto& val : values) {
                    ss << std::format(
                        "{} --> {}\n", static_cast<size_t>(key), static_cast<size_t>(val)
                    );
                }
            }

            return ss.str();
        }
#endif

    private:
        std::unordered_map<KeyVal, std::vector<KeyVal>> m_graph_data{};
    };

} // namespace orb