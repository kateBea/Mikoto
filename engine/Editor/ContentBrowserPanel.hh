//
// Created by kate on 10/8/23.
//

#ifndef MIKOTO_CONTENT_BROWSER_HH
#define MIKOTO_CONTENT_BROWSER_HH

#include <memory>
#include <filesystem>
#include <unordered_map>

#include <Utility/Random.hh>
#include <Editor/Panel.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class ContentBrowserPanel : public Panel {
    public:
        explicit ContentBrowserPanel(Path_T&& root, const Path_T &iconPath = {});
        auto operator=(ContentBrowserPanel && other) -> ContentBrowserPanel & = default;

        auto OnUpdate() -> void override;

        ~ContentBrowserPanel() override = default;

    private:
        auto DrawProjectDirTree(const Path_T& root) -> void;
        auto DrawCurrentDirItems(const Path_T& currentDir) -> Path_T;

    private:
        static constexpr Size_T REQUIRED_IDS{ 1 };
        std::vector<Random::GUID::UUID> m_Guids{};

        std::shared_ptr<Texture2D> m_FolderIcon{};
        std::shared_ptr<Texture2D> m_FileIcon{};

        Path_T m_Root{};
        Path_T m_CurrentDirectory{};
        Path_T m_ForwardDirectory{};
    };
}


#endif//MIKOTO_CONTENT_BROWSER_HH
