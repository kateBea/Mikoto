//
// Created by kate on 10/8/23.
//

#ifndef MIKOTO_CONTENT_BROWSER_HH
#define MIKOTO_CONTENT_BROWSER_HH

#include <filesystem>
#include <memory>
#include <stack>

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
        auto DrawSideView() const -> void;
        auto DrawMainBody() -> void;

        auto OnRightClick() const -> void;

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

        ankerl::unordered_dense::map<TextureIconType, TextureHandle> m_Textures{};
        ankerl::unordered_dense::map<TextureIconType, ImTextureID> m_ImGuiTextureHandles{};
    };
}


#endif//MIKOTO_CONTENT_BROWSER_HH
