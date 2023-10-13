//
// Created by kate on 10/8/23.
//

#include <utility>
#include <filesystem>

#include <volk.h>
#include <imgui.h>
#include <imgui_internal.h>


#include <Utility/StringUtils.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Material/Texture2D.hh>
#include <Editor/ContentBrowserPanel.hh>

#include <ImGui/ImGuiManager.hh>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

namespace Mikoto {
    static constexpr auto GetContentBrowserName() -> std::string_view {
        return "Project";
    }

    ContentBrowserPanel::ContentBrowserPanel(Path_T&& root)
        :   m_Root{ std::move( root ) }
    {
        m_CurrentDirectory = m_Root;
        m_ForwardDirectory = Path_T{};
        m_PanelHeaderName = MakePanelName(ICON_MD_DNS, GetContentBrowserName());

        Size_T entryCount{};
        for (const auto& entry : std::filesystem::directory_iterator(m_Root)) { ++entryCount; }

        for (Size_T count{}; count < REQUIRED_IDS + entryCount; ++count) {
            m_Guids.emplace_back();
        }

        m_FolderIcon = Texture2D::Create("../assets/Icons/folder0.png", MapType::DIFFUSE);
        m_FileIcon = Texture2D::Create("../assets/Icons/file4.png", MapType::DIFFUSE);
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.2f); // Rounded Buttons

        // Settings for the content browser
        if (ImGui::Button(fmt::format("{}", ICON_MD_BUILD).c_str())) {
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
            if (ImGui::Button(fmt::format("{}", ICON_MD_ARROW_BACK_IOS_NEW).c_str())) {
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
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

            if (ImGui::Button(fmt::format("{}", ICON_MD_ARROW_FORWARD_IOS).c_str())) {
                // update forward directory
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
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        }

        ImGui::SameLine();

        ImGui::TextUnformatted(fmt::format("{}", ICON_MD_FOLDER).c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.0f, 0.0f, 0.0f, 0.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.0f, 0.0f, 0.0f, 0.0f });

        // TODO:
        ImGui::SameLine();
        ImGui::Button("Dir1");
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("/");

        ImGui::SameLine();
        ImGui::Button("Dir2");
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("/");

        ImGui::SameLine();
        ImGui::Button("Dir3");
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar();  // Rounded Buttons
    }

    auto ContentBrowserPanel::DrawSideView() -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_FramePadding |
                                                   ImGuiTreeNodeFlags_SpanFullWidth };

        if (ImGui::TreeNodeEx(m_Root.string().c_str(), treeNodeFlags, "Assets")) {

            ImGui::TreePop();
        }
    }

    auto ContentBrowserPanel::DrawMainBody() -> void {
        DrawCurrentDirItems();
    }


    auto ContentBrowserPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            static constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar };
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

        static float padding{ 16.0f };
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

                const auto& icon{ entry.is_directory() ? m_FolderIcon : m_FileIcon };

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                switch (Renderer::GetActiveGraphicsAPI()) {
                    case GraphicsAPI::OPENGL_API:
                        ImGui::ImageButton((ImTextureID)std::any_cast<UInt32_T>(icon->GetImGuiTextureHandle()), ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                        break;
                    case GraphicsAPI::VULKAN_API:
                        ImGui::ImageButton((ImTextureID)std::any_cast<VkDescriptorSet>(icon->GetImGuiTextureHandle()), ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                        break;
                }

                // Save the directory we want to open
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entry.is_directory()) { directoryToOpen = entry.path(); }
                }

                ImGui::PopStyleColor();
                ImGui::TextWrapped("%s", entry.path().stem().c_str());
            }

            ImGui::EndTable();
        }

        m_CurrentDirectory = directoryToOpen;
    }

    auto ContentBrowserPanel::OnRightClick() -> void {
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
    }
}