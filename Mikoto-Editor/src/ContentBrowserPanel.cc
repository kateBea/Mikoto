//
// Created by kate on 10/8/23.
//

#include <utility>
#include <filesystem>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "volk.h"


#include <STL/String/String.hh>

#include "Common/RenderingUtils.hh"
#include "Panels/ContentBrowserPanel.hh"
#include "Material/Texture/Texture2D.hh"
#include "Renderer/Core/Renderer.hh"

#include "GUI/ImGuiManager.hh"

#include "GUI/IconsFontAwesome5.h"
#include "GUI/IconsMaterialDesign.h"
#include "GUI/IconsMaterialDesignIcons.h"

namespace Mikoto {
    static constexpr auto GetContentBrowserName() -> std::string_view {
        return "Project";
    }

    ContentBrowserPanel::ContentBrowserPanel(Path_T&& root)
        :   m_Root{ std::move( root ) }
    {
        m_CurrentDirectory = m_Root;
        m_ForwardDirectory = Path_T{};
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_DNS, GetContentBrowserName());

        Size_T entryCount{};
        for (const auto& entry : std::filesystem::directory_iterator(m_Root)) { ++entryCount; }

        for (Size_T count{}; count < REQUIRED_IDS + entryCount; ++count) {
            m_Guids.emplace_back();
        }

        m_FolderIcon = Texture2D::Create("../Resources/Icons/folder0.png", MapType::TEXTURE_2D_DIFFUSE);
        m_FileIcon = Texture2D::Create("../Resources/Icons/file4.png", MapType::TEXTURE_2D_DIFFUSE);

        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API:
                m_ContentBrowserImTextureIDHandles.emplace(std::make_pair(ContentBrowserTextureIcon::FILE, (ImTextureID)std::any_cast<VkDescriptorSet>(m_FileIcon->GetImGuiTextureHandle())));
                m_ContentBrowserImTextureIDHandles.emplace(std::make_pair(ContentBrowserTextureIcon::FOLDER, (ImTextureID)std::any_cast<VkDescriptorSet>(m_FolderIcon->GetImGuiTextureHandle())));
                break;
        }
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.5f); // Rounded Buttons

        // Settings for the content browser
        if (ImGui::Button(fmt::format("{}", ICON_MD_PRECISION_MANUFACTURING).c_str())) {
            ImGui::OpenPopup("HeaderSettingsPopup");
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (ImGui::BeginPopup("HeaderSettingsPopup")) {
            if (ImGui::Button(fmt::format("{}", ICON_MD_RESTORE).c_str())) {
                m_ThumbnailSize = 128.0f;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            ImGui::SameLine();
            ImGui::Text("Browser thumbnail size");
            ImGui::SliderFloat("##HeaderSettingsPopupThumnailSize", std::addressof(m_ThumbnailSize),90.0f, 256.0f, "%.2f");
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Show folders only in the side tree?
            ImGui::Checkbox("##ShowDirectoriesOnly", std::addressof(m_ShowFoldersOnlyInDirectoryTree));
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            ImGui::SameLine();
            ImGui::Text("Show directories only in side view");

            ImGui::Spacing();
            ImGui::Separator();

            // Show a file hint (small text under file name)
            ImGui::Checkbox("##ShowFileTypeHint", std::addressof(m_ShowFileTypeHint));
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            ImGui::SameLine();
            ImGui::Text("Show file type hint in explorer");

            ImGui::EndPopup();
        }

        // Search bar/filter
        ImGui::SameLine();

        const float cursorPosX{ ImGui::GetCursorPosX() };
        m_SearchFilter.Draw("###ContentBrowserFilter", ImGui::GetContentRegionAvail().x);
        if (!m_SearchFilter.IsActive()) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(cursorPosX + ImGui::GetFontSize() * 0.5f);

            // TODO: grab the color from text color and lower alpha value
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,128));
            ImGui::TextUnformatted(fmt::format("{} Search...", ICON_MD_SEARCH).c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Back button
        {
            bool disabledBackButton{ false };
            if (m_CurrentDirectory == m_Root)
                disabledBackButton = true;

            if (disabledBackButton) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            ImGui::PushFont(ImGuiManager::GetFonts()[2]);
            if (ImGui::Button(fmt::format("{}", ICON_MD_CHEVRON_LEFT).c_str())) {
                m_ForwardDirectory = m_DirectoryStack[m_DirectoryStack.size() - 1];
                m_DirectoryStack.pop_back();

                m_CurrentDirectory = m_DirectoryStack[m_DirectoryStack.size() - 1];
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            ImGui::PopFont();

            if (disabledBackButton) {
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
        }

        ImGui::SameLine();

        // Forward Button
        {
            bool disabledFrontButton{ m_ForwardDirectory.empty() };

            if (disabledFrontButton) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            if (ImGui::Button(fmt::format("{}", ICON_MD_CHEVRON_RIGHT).c_str())) {
                // update forward directory
                m_DirectoryStack.emplace_back(m_ForwardDirectory);
                m_CurrentDirectory = m_ForwardDirectory;

                m_ForwardDirectory = Path_T {};
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (disabledFrontButton) {
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
        }

        ImGui::SameLine();

        // Home directory
        {
            if (ImGui::Button(fmt::format("{}", ICON_MD_HOME).c_str())) {
                m_CurrentDirectory = m_Root;
                m_ForwardDirectory = Path_T {};

                m_DirectoryStack.clear();
                m_DirectoryStack.push_back(m_Root);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        }

        ImGui::SameLine();

        ImGui::TextUnformatted(fmt::format("{}", ICON_MD_FOLDER).c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.16f, 0.16f, 0.16f, 0.5f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.0f, 0.0f, 0.0f, 0.0f });

        // Directory buttons
        bool wantOpenDir{ false };
        auto pathIt{ m_DirectoryStack.begin() };

        for ( ; pathIt != m_DirectoryStack.end(); ++pathIt) {
            ImGui::SameLine();
            if (ImGui::Button(pathIt->stem().string().c_str())) {
                m_CurrentDirectory = *pathIt;
                m_ForwardDirectory = Path_T {};
                wantOpenDir = true;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::SameLine();
            ImGui::Text("/");

            if(wantOpenDir)
                break;
        }

        m_DirectoryStack.erase(pathIt, m_DirectoryStack.end());
        if (m_DirectoryStack.empty()) m_DirectoryStack.push_back(m_Root);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar();  // Rounded Buttons
    }

    auto ContentBrowserPanel::DrawSideView() -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_FramePadding |
                                                   ImGuiTreeNodeFlags_SpanFullWidth };

        for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
            if (entry.is_directory()) {
                if (ImGui::TreeNodeEx(entry.path().string().c_str(), treeNodeFlags, "%s", fmt::format("{} {}", ICON_MD_FOLDER, entry.path().stem().string()).c_str())) {

                    ImGui::TreePop();
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
            }
        }
    }

    auto ContentBrowserPanel::DrawMainBody() -> void {
        DrawCurrentDirItems();
    }


    auto ContentBrowserPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            static constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_None };
            static constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedSame };

            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), windowFlags);

            DrawHeader();

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::Separator();

            const ImVec2 availableRegion{ ImGui::GetContentRegionAvail() };

            if (ImGui::BeginTable("ContentBrowserMainViewTable", 2, tableFlags, availableRegion)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                DrawSideView();

                ImGui::TableNextColumn();
                DrawMainBody();

                ImGui::EndTable();
            }

            OnRightClick();

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

    auto ContentBrowserPanel::DrawCurrentDirItems() -> void {
        Path_T directoryToOpen{ m_CurrentDirectory };

        static const float padding{ 15.0f };
        float cellSize = m_ThumbnailSize + padding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount{ (int)(panelWidth / cellSize) };
        if (columnCount < 1)
            columnCount = 1;


        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit };

        if (ImGui::BeginTable("ContentBrowserCurrentDir", columnCount, flags)) {
            ImGui::TableNextRow();
            for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
                ImGui::TableNextColumn();

                ImTextureID icon{};
                std::string fileType{};

                if (entry.is_directory()) {
                    icon = m_ContentBrowserImTextureIDHandles[ContentBrowserTextureIcon::FOLDER];
                    fileType = "Folder";
                }
                else {
                    // find type (texture, material, text file) file now for simplicity
                    icon = m_ContentBrowserImTextureIDHandles[ContentBrowserTextureIcon::FILE];
                    fileType = "File";
                }

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                if (ImGui::ImageButton(icon, ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 })) {

                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                // Save the directory we want to open
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entry.is_directory()) {
                        directoryToOpen = entry.path();
                        if (m_DirectoryStack.empty()) m_DirectoryStack.emplace_back(m_Root);
                        m_DirectoryStack.emplace_back(entry.path());
                    }
                }

                ImGui::PopStyleColor();
                ImGui::TextWrapped("%s", entry.path().stem().c_str());

                if (m_ShowFileTypeHint) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,128));
                    ImGui::TextUnformatted(fileType.c_str());
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndTable();
        }

        m_CurrentDirectory = directoryToOpen;
    }

    auto ContentBrowserPanel::OnRightClick() -> void {
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
        if (ImGui::BeginPopupContextWindow("ContentBrowserPopup")) {

            ImGui::Spacing();
            if (ImGui::MenuItem(fmt::format("{} Cut", ICON_MD_CONTENT_CUT).c_str(), "Ctrl + X")) {}
            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

            ImGui::Spacing();
            if (ImGui::MenuItem(fmt::format("{} Copy", ICON_MD_CONTENT_COPY).c_str(), "Ctrl + C")) {}
            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

            ImGui::Spacing();
            if (ImGui::MenuItem(fmt::format("{} Paste", ICON_MD_CONTENT_PASTE).c_str(), "Ctrl + P")) {}
            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

            ImGui::Spacing();
            ImGui::Separator();


            ImGui::Spacing();
            if (ImGui::BeginMenu("Add new...")) {

                ImGui::Spacing();
                if (ImGui::MenuItem("Folder")) {
                    ImGui::OpenPopup("ContentBrowserPopupAddNewFolder");
                }

                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }


                if (ImGui::BeginPopupModal("ContentBrowserPopupAddNewFolder")) {
                    ImGui::Text("%s Name:", ICON_FA_SEARCH);
                    static std::array<char, 256> buffer{};

                    if (ImGui::InputText("##ContentBrowserPopupAddNewFolderName", buffer.data(), buffer.size())) {
                        std::filesystem::create_directory(m_CurrentDirectory / Path_T{ buffer.data() });
                    }

                    if (ImGui::Button("Ok")) {
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

                    ImGui::EndPopup();
                }


                ImGui::Spacing();
                if (ImGui::MenuItem("Material")) {}
                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }


                ImGui::Spacing();
                if (ImGui::MenuItem("Regular file")) {}
                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }


                ImGui::EndMenu();
            }


            ImGui::Spacing();
            ImGui::Separator();


            ImGui::Spacing();
            if (ImGui::MenuItem(fmt::format("{} Rename", ICON_MD_DRIVE_FILE_RENAME_OUTLINE).c_str(), "F5")) {}
            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }


            ImGui::Spacing();
            if (ImGui::MenuItem(fmt::format("{} Rename", ICON_MD_DELETE_SWEEP).c_str(), "Delete")) {}
            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }


            ImGui::Spacing();
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
    }
}