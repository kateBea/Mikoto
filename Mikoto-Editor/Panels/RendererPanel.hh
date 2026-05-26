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

#ifndef MIKOTO_RENDERER_PANEL_HH
#define MIKOTO_RENDERER_PANEL_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Panels/Panel.hh>
#include <ImGui/GraphEditor.hh>

namespace mikoto::editor {
    struct EditorState;

    struct RendererPanelCreateInfo {
        EditorState* mState{};
    };

    enum class PresentTargetType {
        eColor,
        eEmissive,
        eNormals,
        ePosition,
        eDepthPrepass,

        eWireframe,

        eFinalImage,

        eCount,
    };

    class RendererPanel final : public Panel {
    public:
        explicit RendererPanel(const RendererPanelCreateInfo& createInfo);

        auto OnUpdate(float timeStep) -> void override;

        ~RendererPanel() override = default;

    private:

        EditorState* mEditorState{};

        PresentTargetType mPresentTargetType{ PresentTargetType::eFinalImage };

        gui::GraphEditor mGraphEditor{ "Pass Graph" };
    };
}


#endif//MIKOTO_RENDERER_PANEL_HH
