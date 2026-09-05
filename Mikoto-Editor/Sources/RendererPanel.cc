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

#include <EASTL/array.h>
#include <EASTL/memory.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/TimeService.hh>
#include <Core/RuntimeConsole.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/GraphEditor.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Memory/Allocator.hh>

#include <Layers/EditorLayer.hh>

#include <Panels/RendererPanel.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    RendererPanel::RendererPanel( const RendererPanelCreateInfo &createInfo )
        : Panel{ "Renderer" },  mEditorState{ createInfo.mState } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_POWER_SETTINGS_NEW, mPanelName );

        // Construct the node graph for pass dependencies
        GraphEditorBuilder builder{};
        auto& nodeControl{ mEditorState->mSceneRenderer->GetNodeControl() };

        for ( const auto& [passName, pass]: nodeControl.mNodes ) {
            builder.PushNode( passName );
        }

        mGraphEditor.Build( builder );
    }

    auto RendererPanel::OnRender( float timeStep ) -> void {
        if (!mPanelIsVisible) {
            return;
        }

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ),
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::SeparatorText( "FrameGraph Info" );

        imgui::DrawNode( "Passes", [this] () -> void {
            imgui::UnindentScoped und{};

        });

        imgui::DrawNode( "Renderer", [this] () -> void {
            // TODO: add aspect ratio, auto, 16:1 etc
        });

        ImGui::SeparatorText( "Renderer Settings" );
        imgui::DrawNode( "Monitor", [this] () -> void {
            imgui::UnindentScoped und{};

            ImGui::SameLine();
            ImGui::TextUnformatted( "Refresh Rate" );
        });

        ImGui::End();
    }
}