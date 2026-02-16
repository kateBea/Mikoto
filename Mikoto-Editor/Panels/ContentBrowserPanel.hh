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

#ifndef MIKOTO_CONTENT_BROWSER_HH
#define MIKOTO_CONTENT_BROWSER_HH

#include <imgui.h>
#include <ankerl/unordered_dense.h>

#include <Panels/Panel.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {

    struct EditorState;

    struct ContentBrowserPanelDescription {
        GpuDevice* Device{ nullptr };
        Path AssetsRootDirectory{ "./Resources" };
        Path ProjectRootDirectory{ "." };

        EditorState* State{ nullptr };
    };

    class ContentBrowserPanel final : public Panel {
    public:

        explicit ContentBrowserPanel(const ContentBrowserPanelDescription& desc);

        auto OnUpdate(float timeStep) -> void override;

        ~ContentBrowserPanel() override = default;

    private:
        enum class TextureIconType : UInt32 {
            ICON_FILE,
            ICON_FOLDER,
            ICON_AUDIO,
            ICON_MATERIAL,
        };

        auto LoadIcons() -> void;
        auto DrawHeader() -> void;
        auto DrawSideView( const Path& root ) -> void;
        auto DrawMainBody() -> void;

        auto OnRightClickBlackSpace() -> void;

        auto DrawCurrentDirItems() -> void;
        auto DrawProjectDirTree(const Path& root ) const -> void;

    private:
        GpuDevice* m_Device{ nullptr };

        ImGuiTextFilter m_SearchFilter{};

        float m_ThumbnailSize{ 100.0f };

        Path m_ProjectRoot{};
        Path m_AssetsRootDirectory{};

        Path m_CurrentDirectory{};
        Path m_ForwardDirectory{};

        bool m_ShowFileTypeHint{};
        bool m_ShowFoldersOnlyInDirectoryTree{};

        std::deque<Path> m_DirectoryStack{};

        EditorState* m_EditorState{ nullptr };

        // NOTE: texture is static because drag and
        // drop needs this to persis
        TextureHandle m_Thumbnail{};

        Path m_SelectedItem{};
        std::vector<Path> m_SelectedItems{};

        ankerl::unordered_dense::set<std::string> m_ThumbnailsUploadCache{};
        ankerl::unordered_dense::map<TextureIconType, TextureHandle> m_Textures{};
        ankerl::unordered_dense::map<TextureIconType, ImTextureID> m_ImGuiTextureHandles{};
    };
}


#endif//MIKOTO_CONTENT_BROWSER_HH
