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
#include <string_view>

#include <imgui.h>
#include <ImGuizmo.h>

# include <imgui_node_editor.h>

#include <GraphNodes/GraphEditor.hh>

namespace Mikoto {

     GraphEditor::GraphEditor( std::string_view name )
        : m_Name{ name } {

         namespace ed = ax::NodeEditor;

         m_Context = ed::CreateEditor(std::addressof( m_Config ) );
     }

    auto GraphEditor::Build( const GraphEditorBuilder& builder ) -> void {
         namespace ed = ax::NodeEditor;
    }

    auto GraphEditor::Render() -> void {
         namespace ed = ax::NodeEditor;

         ImGui::Begin("Graph Editor");
         auto& io = ImGui::GetIO();

         ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

         ImGui::Separator();

         ed::SetCurrentEditor(m_Context);

         ed::Begin("FrameGraph");

         ed::BeginNode(1);
         ImGui::Text("Pass A");
         ed::EndNode();

         ed::BeginNode(2);
         ImGui::Text("Pass B");
         ed::EndNode();

         ed::Link(100, ed::PinId(1), ed::PinId(2));

         ed::End();

         ed::SetCurrentEditor(nullptr);

         ImGui::End();
    }

    auto GraphEditor::GetName() const -> const std::string& {
        return m_Name;
    }
}