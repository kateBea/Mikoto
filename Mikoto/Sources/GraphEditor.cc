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

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_node_editor.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Memory/Allocator.hh>

#include <ImGui/GraphEditor.hh>

namespace mikoto::imgui {

    using namespace mikoto::core;

    static auto MakeId(const eastl::string& key) -> core::i32 {
        return as<core::i32>( eastl::hash<eastl::string>{}( key ) );
    }

    auto GraphEditorBuilder::PushNode( eastl::string_view node ) -> void {
        const auto key{ string::From( node ) };
        if ( !mNodes.contains( key ) ) {
            GraphNode nodeData{
                .mID = MakeId( key ),
                .mKey{ string::From(node) },
                .mDisplayName{ key },
            };

            mNodes.emplace(nodeData.mKey, std::move(nodeData));
        }
    }

    GraphEditor::GraphEditor( eastl::string_view name )
        : mName{ name } {

        namespace ed = ax::NodeEditor;

        // The library saves the layout to a JSON, if we do not specify
        // one it uses one called NodeEditor or something by default
        // This can make it so that widgets etc don't show up because of not proper layout saved
        mConfigLayoutFile = string::Format( "{}.json", mName);
        mConfig.SettingsFile = mConfigLayoutFile.c_str();
        mConfig.EnableSmoothZoom = true;
        mContext = ed::CreateEditor( std::addressof( mConfig ) );
    }

    auto GraphEditor::Build( GraphEditorBuilder& builder ) -> void {
        namespace ed = ax::NodeEditor;

        mNodes = std::move( builder.mNodes );
    }

    auto GraphEditor::Render( bool& open ) -> void {
        namespace ed = ax::NodeEditor;

        ImGui::Begin("Graph Editor", MKT_ADDRESSOF( open ));

        auto& io{ ImGui::GetIO() };

        ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

        ImGui::Separator();

        ed::SetCurrentEditor(mContext);

        ed::Begin("FrameGraph");

        for ( const auto& [name, node] : mNodes) {
            RenderNode( node );
        }

        ed::End();

        ed::SetCurrentEditor(nullptr);

        ImGui::End();
    }

    auto GraphEditor::GetName() const -> const eastl::string& {
        return mName;
    }

    auto GraphEditor::RenderNode( const GraphNode& node ) -> void {
        namespace ed = ax::NodeEditor;
        ed::BeginNode(node.mID);


        ImGui::TextUnformatted(node.mDisplayName.c_str());

        ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.3f, 1.0f ), "Reads" );
        for ( const auto& input: node.mInputs ) {
            ImGui::TextUnformatted( "%s", input.c_str() );
        }

        ImGui::TextColored( ImVec4( 0.5f, 0.25f, 0.25f, 1.0f ), "Writes");
        for ( const auto& output: node.mOutputs ) {
            ImGui::TextUnformatted( "%s", output.c_str() );
        }

        ed::EndNode();
    }
}