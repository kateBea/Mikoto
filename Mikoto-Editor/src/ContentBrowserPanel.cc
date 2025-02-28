//
// Created by kate on 10/8/23.
//

#include "Panels/ContentBrowserPanel.hh"

#include <GUI/Icons/IconsMaterialDesignIcons.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#include <volk.h>

#include <Core/Engine.hh>
#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/GUISystem.hh>
#include <Core/System/RenderSystem.hh>
#include <GUI/ImGuiUtils.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/String/String.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <filesystem>
#include <utility>

#include "GUI/Icons/IconsFontAwesome5.h"
#include "GUI/Icons/IconsMaterialDesign.h"
#include "Material/Texture/Texture2D.hh"

namespace Mikoto {
    static constexpr auto GetContentBrowserName() -> std::string_view {
        return "Project";
    }

    static auto CreateSampler() -> VkSampler {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VkSamplerCreateInfo samplerInfo{ VulkanHelpers::Initializers::SamplerCreateInfo() };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        const VkPhysicalDeviceProperties& properties{ device.GetPhysicalDeviceProperties() };

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VkSampler sampler{};

        if ( vkCreateSampler( device.GetLogicalDevice(), &samplerInfo, nullptr, &sampler ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create texture sampler!" );
        }

        VulkanDeletionQueue::Push( [sampler = sampler, device = device.GetLogicalDevice()]() -> void {
            vkDestroySampler( device, sampler, nullptr );
        } );

        return sampler;
    }

    ContentBrowserPanel::ContentBrowserPanel()
        : Panel{ StringUtils::MakePanelName( ICON_MD_DNS, GetContentBrowserName() ) }
    {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
        m_AssetsRoot = fileSystem.GetAssetsRootPath();

        LoadIconsTexturesHandles();

        m_CurrentDirectory = m_AssetsRoot;
        m_ForwardDirectory = Path_T{};
    }

    auto ContentBrowserPanel::LoadIconsTexturesHandles() -> void {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

        const TextureLoadInfo file1TextLoadInfo{
            .Path{ PathBuilder()
                        .WithPath( fileSystem.GetIconsRootPath().string() )
                        .WithPath( "file4.png" )
                        .Build() },
            .Type{ MapType::TEXTURE_2D_DIFFUSE },
        };

        m_FileIcon = dynamic_cast<Texture2D*>( assetsSystem.LoadTexture( file1TextLoadInfo ) );

        const TextureLoadInfo folder1TextLoadInfo{
            .Path{ PathBuilder()
                        .WithPath( fileSystem.GetIconsRootPath().string() )
                        .WithPath( "folder0.png" )
                        .Build() },
            .Type{ MapType::TEXTURE_2D_DIFFUSE },
        };

        m_FolderIcon = dynamic_cast<Texture2D*>( assetsSystem.LoadTexture( folder1TextLoadInfo ) );

        const VkSampler fileSampler{ CreateSampler() };
        const VkSampler folderSampler{ CreateSampler() };

        VkDescriptorSet fileDs{ ImGui_ImplVulkan_AddTexture(fileSampler, dynamic_cast<VulkanTexture2D*>(m_FileIcon)->GetImage().GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) };
        VkDescriptorSet folderDs{ ImGui_ImplVulkan_AddTexture(folderSampler, dynamic_cast<VulkanTexture2D*>(m_FolderIcon)->GetImage().GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) };

        GUISystem& guiSystem{ Engine::GetSystem<GUISystem>() };

        guiSystem.AddShutdownCallback( [fileDs, folderDs]() -> void {
            ImGui_ImplVulkan_RemoveTexture( fileDs );
            ImGui_ImplVulkan_RemoveTexture( folderDs );
        } );

        m_ContentBrowserImTextureIDHandles.emplace( std::make_pair( ContentBrowserTextureIcon::FILE, reinterpret_cast<ImTextureID>( fileDs ) ) );
        m_ContentBrowserImTextureIDHandles.emplace( std::make_pair( ContentBrowserTextureIcon::FOLDER, reinterpret_cast<ImTextureID>( folderDs ) ) );
    }

    auto ContentBrowserPanel::DrawHeader() -> void {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.5f); // Rounded Buttons

        // Settings for the content browser
        if (ImGui::Button(fmt::format("{}", ICON_MD_SETTINGS_APPLICATIONS).c_str())) {
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

        ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.0f };

        // Back button
        {
            bool disabledBackButton{ false };
            if (m_CurrentDirectory == m_AssetsRoot)
                disabledBackButton = true;

            if (disabledBackButton) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            GUISystem& guiSystem{ Engine::GetSystem<GUISystem>() };

            ImGui::PushFont(guiSystem.GetFonts()[2]);
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
                m_CurrentDirectory = m_AssetsRoot;
                m_ForwardDirectory = Path_T {};

                m_DirectoryStack.clear();
                m_DirectoryStack.push_back(m_AssetsRoot);
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
        if (m_DirectoryStack.empty()) m_DirectoryStack.push_back(m_AssetsRoot);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar();  // Rounded Buttons
    }

    auto ContentBrowserPanel::DrawSideView() const -> void {
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
            static constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable | ImGuiWindowFlags_NoCollapse | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedSame };

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

    auto ContentBrowserPanel::DrawProjectDirTree(const Path_T& root ) const -> void {
        constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_None };
        constexpr ImGuiTreeNodeFlags childNodeFlags{ styleFlags | ImGuiTreeNodeFlags_DefaultOpen };

        if (ImGui::TreeNodeEx( reinterpret_cast<const void*>( root.string().c_str() ), childNodeFlags, "%s", root.stem().c_str())) {
            for (const auto& entry : std::filesystem::directory_iterator(root)) {
                if (entry.is_directory()) {
                    if (ImGui::TreeNodeEx( reinterpret_cast<const void*>( entry.path().string().c_str() ), childNodeFlags, "%s", entry.path().stem().c_str())) {

                        ImGui::TreePop();
                    }
                }
            }

            ImGui::TreePop();
        }
    }

    auto ContentBrowserPanel::DrawCurrentDirItems() -> void {
        Path_T directoryToOpen{ m_CurrentDirectory };

        constexpr float padding{ 15.0f };
        const float cellSize{ m_ThumbnailSize + padding };

        const float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount{ static_cast<int>( panelWidth / cellSize ) };
        if (columnCount < 1) {
            columnCount = 1;
        }

        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit };

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
                if (ImGui::ImageButton(entry.path().string().c_str(), icon, ImVec2{ m_ThumbnailSize, m_ThumbnailSize }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 })) {

                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                // Save the directory we want to open
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entry.is_directory()) {
                        directoryToOpen = entry.path();
                        if (m_DirectoryStack.empty()) {
                            m_DirectoryStack.emplace_back(m_AssetsRoot);
                        }

                        m_DirectoryStack.emplace_back(entry.path());
                    }
                }

                // File name
                ImGui::PopStyleColor();
                ImGuiUtils::CenteredText(fmt::format( "{}", entry.path().stem().string()).c_str(), m_ThumbnailSize);

                // Type of file
                if (m_ShowFileTypeHint) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,128));
                    ImGuiUtils::CenteredText( fmt::format( "{}", fileType.c_str() ).c_str(), m_ThumbnailSize);
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndTable();
        }

        m_CurrentDirectory = directoryToOpen;
    }

    auto ContentBrowserPanel::OnRightClick() const -> void {
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