//
// Created by kate on 10/8/23.
//

#ifndef MIKOTO_CONTENT_BROWSER_HH
#define MIKOTO_CONTENT_BROWSER_HH

#include <memory>
#include <stack>
#include <filesystem>
#include <unordered_map>

#include "imgui.h"

#include "STL/Random/Random.hh"
#include "Panel.hh"
#include "Material/Texture/Texture2D.hh"

namespace Mikoto {
    class ContentBrowserPanel final : public Panel {
    public:
        explicit ContentBrowserPanel(Path_T&& root);
        auto operator=(ContentBrowserPanel && other) -> ContentBrowserPanel & = default;

        auto OnUpdate(float timeStep) -> void override;

        ~ContentBrowserPanel() override = default;

    private:
        enum class ContentBrowserTextureIcon {
            FILE,
            FOLDER,
            AUDIO,
            MATERIAL,
        };

        auto DrawHeader() -> void;
        auto DrawSideView() -> void;
        auto DrawMainBody() -> void;

        auto OnRightClick() -> void;

        auto DrawProjectDirTree(const Path_T& root) -> void;
        auto DrawCurrentDirItems() -> void;

    private:
        static constexpr Size_T REQUIRED_IDS{ 1 };
        std::vector<UUID> m_Guids{};

        std::shared_ptr<Texture2D> m_FolderIcon{};
        std::shared_ptr<Texture2D> m_FileIcon{};

        ImGuiTextFilter m_SearchFilter{};

        float m_ThumbnailSize{ 100.0f };

        Path_T m_Root{};
        Path_T m_CurrentDirectory{};
        Path_T m_ForwardDirectory{};

        bool m_ShowFoldersOnlyInDirectoryTree{};
        bool m_ShowFileTypeHint{};

        std::vector<Path_T> m_DirectoryStack{};

        std::unordered_map<ContentBrowserTextureIcon, ImTextureID> m_ContentBrowserImTextureIDHandles{};
    };
}


#endif//MIKOTO_CONTENT_BROWSER_HH
