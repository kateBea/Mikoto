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

#ifndef MIKOTO_PANELS_WORLD_SELECTOR_HH
#define MIKOTO_PANELS_WORLD_SELECTOR_HH

#include <imgui.h>

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::editor {

    // Browses the bundled demo manifests and returns the chosen path to the project launcher.
    class WorldSelector final {
    public:
        auto Open() -> void;
        auto Close() -> void;

        MKT_NODISCARD auto IsOpen() const -> bool;
        MKT_NODISCARD auto Draw( renderer::rhi::IGpuDevice* device, ImVec2 workspacePosition, ImVec2 workspaceSize,
            bool dimWorkspace ) -> filesystem::Path;
        MKT_NODISCARD static auto FindDemoProjects() -> eastl::vector<filesystem::Path>;

    private:
        struct World {
            filesystem::Path mProject{};
            filesystem::Path mPreviewPath{};
            eastl::string mFallbackTitle{};
            eastl::string mTitleKey{};
            eastl::string mDescriptionKey{};
            renderer::rhi::TextureHandle mPreview{};
            bool mPreviewChecked{};
        };

        auto Refresh() -> void;

        auto DrawSearch() -> void;
        auto DrawGrid( renderer::rhi::IGpuDevice* device, core::f32 width ) -> filesystem::Path;
        auto DrawDetails( renderer::rhi::IGpuDevice* device ) -> filesystem::Path;

        MKT_NODISCARD auto DrawCard( World& world, renderer::rhi::IGpuDevice* device, core::f32 width ) -> bool;
        MKT_NODISCARD auto FindSelection() -> World*;
        MKT_NODISCARD auto PassesFilter( const World& world ) const -> bool;
        MKT_NODISCARD static auto GetTitle( const World& world ) -> const eastl::string&;
        MKT_NODISCARD static auto GetDescription( const World& world ) -> const eastl::string&;
        MKT_NODISCARD static auto GetPreviewID( World& world, renderer::rhi::IGpuDevice* device ) -> ImTextureID;

    private:
        eastl::vector<World> mWorlds{};
        ImGuiTextFilter mSearch{};
        filesystem::Path mSelectedProject{};

        bool mVisible{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_WORLD_SELECTOR_HH
