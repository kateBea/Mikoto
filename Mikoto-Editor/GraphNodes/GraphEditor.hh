//    Copyright 2025 ケイト
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

#ifndef MIKOTO_GRAPH_EDITOR_HH
#define MIKOTO_GRAPH_EDITOR_HH
#include <string>
#include <string_view>
#include <vector>

#include <imgui_node_editor.h>
#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct GraphNode {
        Int32 ID{};
        std::string Key{};
        std::string DisplayName{};

        std::vector<std::string> Inputs{};
        std::vector<std::string> Outputs{};
    };

    class GraphEditorBuilder {
    public:

        auto PushNode( std::string_view node ) -> void;

    private:
        friend class GraphEditor;

    private:
        ankerl::unordered_dense::map<std::string, GraphNode> m_Nodes{};
    };

    // TODO: investigate how we can properly use multiple
    class GraphEditor {
    public:
        explicit GraphEditor( std::string_view name );

        auto Render() -> void;

        auto Build( GraphEditorBuilder& builder ) -> void;

        MKT_NODISCARD auto GetName() const -> const std::string&;

    private:
        static auto RenderNode( const GraphNode& node ) -> void;

    private:
        std::string m_Name{};
        std::string m_ConfigLayoutFile{};

        ax::NodeEditor::Config m_Config{};
        ax::NodeEditor::EditorContext* m_Context{};

        ankerl::unordered_dense::map<std::string, GraphNode> m_Nodes{};
    };
}

#endif// MIKOTO_GRAPH_EDITOR_HH
