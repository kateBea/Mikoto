//
// Created by kate on 10/8/23.
//

#include <utility>
#include <filesystem>

#include <volk.h>
#include <imgui.h>

#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Material/Texture2D.hh>
#include <Editor/ContentBrowserPanel.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsFontAwesome5.h>

namespace Mikoto {
    ContentBrowserPanel::ContentBrowserPanel(Path_T&& root, const Path_T &iconPath)
        :   m_Root{ std::move( root ) }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;

        m_CurrentDirectory = m_Root;
        m_ForwardDirectory = m_Root;

        Size_T entryCount{};
        for (const auto& entry : std::filesystem::directory_iterator(m_Root)) { ++entryCount; }

        for (Size_T count{}; count < REQUIRED_IDS + entryCount; ++count) {
            m_Guids.emplace_back();
        }

        m_FolderIcon = Texture2D::Create("../assets/Icons/folder9.png", MapType::DIFFUSE);
        m_FileIcon = Texture2D::Create("../assets/Icons/file4.png", MapType::DIFFUSE);
    }

    auto ContentBrowserPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {

            static constexpr Int32_T COLUM_COUNT{ 2 };
            static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PreciseWidths };

            ImGui::Begin("Content Browser");

            if (ImGui::BeginTable("ContentBrowserTable", COLUM_COUNT, flags)) {
                ImGui::TableNextRow();

#if 0
                // Project directory hierarchy
                ImGui::TableNextColumn();
                DrawProjectDirTree(m_Root);
#endif

                // Content browser
                ImGui::TableNextColumn();
                if (ImGui::Button(fmt::format("{}", ICON_MD_ARROW_BACK_IOS_NEW).c_str(), ImVec2{ 30, 24 })) {
                    m_ForwardDirectory = m_CurrentDirectory;
                    m_CurrentDirectory = m_CurrentDirectory == m_Root ? m_Root : m_CurrentDirectory.parent_path();
                }

                ImGui::SameLine();

                if (ImGui::Button(fmt::format("{}", ICON_MD_ARROW_FORWARD_IOS).c_str(), ImVec2{ 30, 24 })) {
                    m_CurrentDirectory = m_ForwardDirectory;
                }

                ImGui::SameLine();

                if (ImGui::Button(fmt::format("{}", ICON_MD_HOME).c_str(), ImVec2{ 30, 24 })) {
                    m_CurrentDirectory = m_Root;
                }

                ImGui::SameLine();

                static std::array<char, 256> data{};
                if (ImGui::InputText("##ContentBrowserLookup", data.data(), 50)) { }

                ImGui::SameLine();

                if (ImGui::Button(fmt::format("{}", ICON_MD_SEARCH).c_str(), ImVec2{ 30, 24 })) {

                }

                m_CurrentDirectory = DrawCurrentDirItems(m_CurrentDirectory);

                ImGui::EndTable();
            }

            const ImGuiPopupFlags popupFlags { ImGuiPopupFlags_None };
            if (ImGui::BeginPopupContextWindow("ContentBrowserPopup")) {

                if (ImGui::MenuItem("Cut", "Ctrl + X")) {}
                if (ImGui::MenuItem("Copy", "Ctrl + C")) {}
                if (ImGui::MenuItem("Paste", "Ctrl + P")) {}

                ImGui::Separator();

                if (ImGui::BeginMenu("Add new...")) {
                    if (ImGui::MenuItem("Folder")) {
                        ImGui::OpenPopup("ContentBrowserPopupAddNewFolder");
                    }

                    if (ImGui::BeginPopupModal("ContentBrowserPopupAddNewFolder")) {
                        ImGui::Text("%s Name:", ICON_FA_SEARCH);
                        static std::array<char, 256> buffer{};

                        if (ImGui::InputText("##ContentBrowserPopupAddNewFolderName", buffer.data(), buffer.size())) {
                            std::filesystem::create_directory(m_CurrentDirectory / Path_T{ buffer.data() });
                        }

                        if (ImGui::Button("Ok"))
                            ImGui::CloseCurrentPopup();

                        ImGui::EndPopup();
                    }

                    if (ImGui::MenuItem("Material")) {}
                    if (ImGui::MenuItem("Regular file")) {}
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Rename", "F5")) {}
                if (ImGui::MenuItem("Delete", "Delete")) {}


                ImGui::EndPopup();
            }

            ImGui::End();
        }
    }

    auto ContentBrowserPanel::DrawProjectDirTree(const Path_T& root) -> void {
        const ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_None };

        ImGuiTreeNodeFlags childNodeFlags{ styleFlags | ImGuiTreeNodeFlags_DefaultOpen };

        if (ImGui::TreeNodeEx((void*)m_Guids[0].Get(), childNodeFlags, "%s", root.stem().c_str())) {
            Size_T count{ 1 };
            for (auto& entry : std::filesystem::directory_iterator(root)) {
                if (entry.is_directory()) {
                    if (ImGui::TreeNodeEx((void*)m_Guids[count++].Get(), childNodeFlags, "%s", entry.path().stem().c_str())) { ImGui::TreePop(); }
                }
            }

            ImGui::TreePop();
        }
    }

    auto ContentBrowserPanel::DrawCurrentDirItems(const Path_T& currentDir) -> Path_T {
        Path_T result{ currentDir };
        const Int32_T COLUM_COUNT{ 7 };
        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchProp };

        if (ImGui::BeginTable("ContentBrowserCurrentDir", COLUM_COUNT, flags)) {
            ImGui::TableNextRow();
            for (auto& entry : std::filesystem::directory_iterator(currentDir)) {
                ImGui::TableNextColumn();

                const auto& icon{ entry.is_directory() ? m_FolderIcon : m_FileIcon };

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                switch (Renderer::GetActiveGraphicsAPI()) {
                    case GraphicsAPI::OPENGL_API:
                        ImGui::ImageButton((ImTextureID)std::any_cast<UInt32_T>(icon->GetImGuiTextureHandle()), ImVec2{ 100, 100 }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                        break;
                    case GraphicsAPI::VULKAN_API:
                        ImGui::ImageButton((ImTextureID)std::any_cast<VkDescriptorSet>(icon->GetImGuiTextureHandle()), ImVec2{ 100, 100 }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                        break;
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entry.is_directory()) { result = entry.path(); }
                }

                ImGui::PopStyleColor();
                ImGui::TextWrapped("%s", entry.path().stem().c_str());
            }

            ImGui::EndTable();
        }

        return result;
    }
}