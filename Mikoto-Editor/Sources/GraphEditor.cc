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

#include <cmath>
#include <string>
#include <string_view>

#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_node_editor.h>

#include <Common/String.hh>
#include <GraphNodes/GraphEditor.hh>

namespace Mikoto {

    static auto MakeId(const std::string& key) -> Int32 {
        return static_cast<Int32>( std::hash<std::string>{}( key ) );
    }

    auto GraphEditorBuilder::PushNode( std::string_view node ) -> void {
        const auto key{ StringUtil::From( node ) };
        if ( !m_Nodes.contains( key ) ) {
            GraphNode nodeData{
                .ID{ MakeId( key ) },
                .Key{ StringUtil::From(node) },
                .DisplayName{ key },
            };

            m_Nodes.emplace(nodeData.Key, std::move(nodeData));
        }
    }

    GraphEditor::GraphEditor( std::string_view name )
        : m_Name{ name } {

        namespace ed = ax::NodeEditor;

        // The library saves the layout to a JSON, if we do not specify
        // one it uses one called NodeEditor or something by default
        // This can make it so that widgets etc don't show up because of not proper layout saved
        m_ConfigLayoutFile = StringUtil::Format( "{}.json", m_Name);
        m_Config.SettingsFile = m_ConfigLayoutFile.c_str();
        m_Config.EnableSmoothZoom = true;
        m_Context = ed::CreateEditor( std::addressof( m_Config ) );
    }

    auto GraphEditor::Build( const GraphEditorBuilder& builder ) -> void {
        namespace ed = ax::NodeEditor;

        m_Nodes = std::move( builder.m_Nodes );
    }

    auto GraphEditor::Render() -> void {
        namespace ed = ax::NodeEditor;

        ImGui::Begin("Graph Editor");
        auto& io = ImGui::GetIO();

        ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

        ImGui::Separator();

        ed::SetCurrentEditor(m_Context);

        ed::Begin("FrameGraph");

        for ( const auto& [name, node] : m_Nodes) {
            RenderNode( node );
        }

        ed::End();

        ed::SetCurrentEditor(nullptr);

        ImGui::End();
    }

    auto GraphEditor::GetName() const -> const std::string& {
        return m_Name;
    }

    auto GraphEditor::RenderNode( const GraphNode& node ) -> void {
        namespace ed = ax::NodeEditor;
        ed::BeginNode(node.ID);


        ImGui::TextUnformatted(node.DisplayName.c_str());

        ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.3f, 1.0f ), "Reads" );
        for ( const auto& input: node.Inputs ) {
            ImGui::TextUnformatted( "%s", input.c_str() );
        }

        ImGui::TextColored( ImVec4( 0.5f, 0.25f, 0.25f, 1.0f ), "Writes");
        for ( const auto& output: node.Outputs ) {
            ImGui::TextUnformatted( "%s", output.c_str() );
        }

        ed::EndNode();
    }
}