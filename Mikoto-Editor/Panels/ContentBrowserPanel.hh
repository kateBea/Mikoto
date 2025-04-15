//
// Created by kate on 10/8/23.
//

#ifndef MIKOTO_CONTENT_BROWSER_HH
#define MIKOTO_CONTENT_BROWSER_HH

#include <memory>
#include <filesystem>
#include <ankerl/unordered_dense.h>

#include <imgui.h>

#include <Panels/Panel.hh>
#include <Library/Random/Random.hh>
#include <Renderer/GpuDevice.hh>

namespace Mikoto {

    struct ContentBrowserPanelDescription {
        GpuDevice* Device{ nullptr };
        Path_T AssetsRootDirectory{};
        Path_T ProjectRootDirectory{};
    };

    class ContentBrowserPanel final : public Panel {
    public:

        explicit ContentBrowserPanel(const ContentBrowserPanelDescription& desc);
        auto operator=(ContentBrowserPanel && other) -> ContentBrowserPanel & = default;

        auto OnUpdate(float timeStep) -> void override;

        ~ContentBrowserPanel() override = default;

    private:
        enum class TextureIconType : UInt32_T {
            ICON_FILE,
            ICON_FOLDER,
            ICON_AUDIO,
            ICON_MATERIAL,
        };

        auto LoadIcons() -> void;
        auto DrawHeader() -> void;
        auto DrawSideView() const -> void;
        auto DrawMainBody() -> void;

        auto OnRightClick() const -> void;

        auto DrawCurrentDirItems() -> void;
        auto DrawProjectDirTree(const Path_T& root ) const -> void;

    private:
        GpuDevice* m_Device{ nullptr };

        TextureHandle m_FolderIcon{};
        TextureHandle m_FileIcon{};

        ImGuiTextFilter m_SearchFilter{};

        float m_ThumbnailSize{ 100.0f };

        Path_T m_ProjectRoot{};
        Path_T m_AssetsRootDirectory{};

        Path_T m_CurrentDirectory{};
        Path_T m_ForwardDirectory{};

        bool m_ShowFileTypeHint{};
        bool m_ShowFoldersOnlyInDirectoryTree{};

        std::vector<Path_T> m_DirectoryStack{};

        ankerl::unordered_dense::map<TextureIconType, ImTextureID> m_ContentBrowserImTextureIDHandles{};
    };
}


#endif//MIKOTO_CONTENT_BROWSER_HH
