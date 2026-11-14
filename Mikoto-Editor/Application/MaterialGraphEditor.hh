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

#ifndef MIKOTO_EDITOR_MATERIAL_GRAPH_EDITOR_HH
#define MIKOTO_EDITOR_MATERIAL_GRAPH_EDITOR_HH

#include <EASTL/unordered_map.h>

#include <Assets/AssetTexture.hh>
#include <Assets/AssetLoadRegistry.hh>

#include <Material/MaterialGraph.hh>

#include <Renderer/Rhi/Texture.hh>

#include <ImGui/GraphEditor.hh>


namespace mikoto::editor {
    /** @brief Shared material authoring canvas. Returns edits without touching rendered materials. */
    class MaterialGraphEditor final {
    public:
        /** @brief Discards editor-only selection/layout/texture demand when switching assets. */
        auto Reset() -> void;
        /** @brief Edits connections, values, texture previews and named comment groups. */
        auto Render( material::MaterialGraph& graph, core::cstr surfaceLabel = nullptr ) -> bool;

    private:
        imgui::NodeEditorContext mContext{};
        struct TexturePreview {
            asset::AssetHandle<asset::Texture> mRequest{};
            eastl::string mError{};
        };
        eastl::unordered_map<material::NodeID, TexturePreview> mTextures{};
        bool mRestoreLayout{ true };
        material::NodeID mRenamingNode{};
        eastl::string mStatus{};
        eastl::string mGeneratedYaml{};
        bool mShowGeneratedYaml{};
    };
}// namespace mikoto::editor
#endif

