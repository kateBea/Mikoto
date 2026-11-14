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

#ifndef MIKOTO_EDITOR_LANDSCAPE_PANEL_HH
#define MIKOTO_EDITOR_LANDSCAPE_PANEL_HH

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Core/TerrainGeneration.hh>

#include <Scene/Scene.hh>

#include <Panels/Panel.hh>

#include <FileSystem/Path.hh>

namespace mikoto::editor {

    struct EditorState;

    struct LandscapePanelCreateInfo {
        EditorState *mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class LandscapePanel final : public Panel {
    public:

        explicit LandscapePanel( const LandscapePanelCreateInfo& info );

        auto OnRender( float timeStep ) -> void override;

    private:
        auto UpdateHeightmapPreview( const eastl::shared_ptr<const renderer::TerrainHeightmap>& heightmap ) -> void;
        EditorState *mEditorState{};
        const scene::Scene* mEditedScene{};
        core::u64 mEditedEntity{};
        filesystem::Path mImportPath{};
        eastl::string mEditError{};
        eastl::shared_ptr<const renderer::TerrainHeightmap> mPreviewSource{};
        renderer::rhi::TextureHandle mHeightmapPreview{};
        eastl::string mPreviewError{};
        eastl::string mMapSaveMessage{};
        bool mPreviewSelected{};
    };
}

#endif//MIKOTO_EDITOR_LANDSCAPE_PANEL_HH
