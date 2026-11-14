//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_CORE_GRAPH_HH
#define MIKOTO_CORE_GRAPH_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::core {

    enum class GraphType {
        eDirected,
        eUndirected
    };

    /**
     * Small graph for diagnostics and DOT export, independent of the renderer.
     * Labels are owned UTF-8 text. Node IDs are insertion indices, so repeated
     * labels remain distinct nodes. Cycles, self-edges and parallel edges are valid.
     *
     * DotLanguage: https://www.youtube.com/watch?v=uA2coCHpH0A
     */
    class DotGraph final {
    public:
        using NodeID = usize;

        /**
         * @brief Constructs an empty graph.
         *
         * @param name Name used when exporting the graph to DOT.
         * @param type Graph type, determining whether edges are directed or
         *             undirected.
         */
        explicit DotGraph( eastl::string_view name = "Graph", GraphType type = GraphType::eDirected );


        /**
         * @brief Adds a node to the graph.
         *
         * The label is copied and owned by the graph. Multiple nodes may use the
         * same label and will still receive distinct identifiers.
         *
         * @param label UTF-8 label associated with the node.
         *
         * @return Identifier of the newly created node.
         *
         * @note The returned identifier remains valid until Clear() is called.
         */
        MKT_NODISCARD auto AddNode( eastl::string_view label ) -> NodeID;


        /**
         * @brief Adds an edge between two existing nodes.
         *
         * Cycles, self-edges, and parallel edges are permitted.
         *
         * @param from  Source node identifier.
         * @param to    Destination node identifier.
         * @param label Optional UTF-8 label associated with the edge.
         *
         * @return `true` if the edge was added successfully.
         * @return `false` if either node identifier does not belong to the graph.
         *
         * @note The graph is left unchanged when either endpoint is invalid.
         */
        MKT_NODISCARD auto AddEdge( NodeID from, NodeID to, eastl::string_view label = {} ) -> bool;


        /**
         * @brief Removes all nodes and edges from the graph.
         *
         * The graph name and graph type are preserved.
         *
         * @note All previously returned NodeID values become invalid.
         */
        auto Clear() -> void;


        /**
         * @brief Generates a complete Graphviz DOT representation of the graph.
         *
         * Nodes and edges are emitted in insertion order. No graph traversal is
         * performed or required.
         *
         * @return Complete DOT document representing the graph.
         */
        MKT_NODISCARD auto ToDot() const -> eastl::string;


        /**
         * @brief Writes the graph to standard output in Graphviz DOT format.
         *
         * The output is equivalent to ToDot() and is written directly to stdout
         * without logging prefixes, timestamps, or ANSI color sequences.
         */
        auto Dump() const -> void;

    private:
        struct Node {
            eastl::string mLabel{};
        };

        struct Edge {
            NodeID mFrom{};
            NodeID mTo{};
            eastl::string mLabel{};
        };

    private:
        eastl::string mName{};
        eastl::vector<Node> mNodes{};
        eastl::vector<Edge> mEdges{};
        GraphType mType{ GraphType::eDirected };
    };

}// namespace mikoto::core

#endif// MIKOTO_CORE_GRAPH_HH
