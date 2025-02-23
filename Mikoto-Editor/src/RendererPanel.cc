//
// Created by kate on 10/13/23.
//

#include "Panels/RendererPanel.hh"

#include "imgui.h"

#include <GUI/RenderViewport.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

#include "GUI/IconsMaterialDesign.h"
#include "GUI/ImGuiManager.hh"

namespace Mikoto {
    struct RenderPanelViewport_VkImplCreateInfo {
        RenderViewportCreateInfo ViewportCreateInfo{};
    };

    class RenderPanelViewport_VkImpl final : public RenderViewport {
    public:
        explicit RenderPanelViewport_VkImpl( const RenderPanelViewport_VkImplCreateInfo& createInfo)
            : RenderViewport{ createInfo.ViewportCreateInfo }
        {}

        auto Init() -> void override {

            // Create a Sampler for the texture we will display in the viewport
            VkSamplerCreateInfo colorSamplerCreateInfo{ GetDefaultSamplerCreateInfo() };

            if ( vkCreateSampler( VulkanContext::Get().GetDevice().GetLogicalDevice(), &colorSamplerCreateInfo, nullptr, &m_ColorAttachmentSampler ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "RenderPanelViewport_VkImpl:Init - Failed to create color Vulkan sampler!" );
            }

            VulkanDeletionQueue::Push( [colorSampler = m_ColorAttachmentSampler]() -> void {
                vkDestroySampler( VulkanContext::Get().GetDevice().GetLogicalDevice(), colorSampler, nullptr );
            } );

            // Create the Descriptor set for the texture displayed in the ImGuiWindow scene

            const VulkanRenderer* vulkanSceneRenderer{ dynamic_cast<const VulkanRenderer*>( m_Renderer ) };

            m_ColorAttachmentDescriptorSet =
                    ImGui_ImplVulkan_AddTexture( m_ColorAttachmentSampler, vulkanSceneRenderer->GetFinalImage().GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

            ImGuiManager::AddShutdownCallback( [colorDs = m_ColorAttachmentDescriptorSet]() -> void {
                ImGui_ImplVulkan_RemoveTexture( colorDs );
            } );
        }

        auto ShowPasses( ImGuiTreeNodeFlags treeNodeFlags ) -> void {

            if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "RenderPanelViewport_VkImpl::ShowPasses" ), treeNodeFlags, "%s", "Passes")) {

                // Handle type of projection
                ImGui::Spacing();
                if ( ImGui::BeginCombo( "##RenderPanelViewport_VkImpl::ShowPassesCombo", s_ImageCompositions[m_CurrentPassSelectionIndex] ) ) {

                    for ( Size_T index{}; index < s_ImageCompositions.size(); ++index ) {
                        const std::string composition{ s_ImageCompositions[index] };

                        const bool isSelected{ StringUtils::Equal( composition , s_ImageCompositions[m_CurrentPassSelectionIndex] ) };

                        if ( ImGui::Selectable( fmt::format( " {}", composition ).c_str(), isSelected ) ) {
                            m_CurrentPassSelectionIndex = index;
                        }

                        if ( ImGui::IsItemHovered() ) {
                            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                        }

                        if ( isSelected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                                        }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                const ImVec2 viewPortDimensions{ ImGui::GetContentRegionAvail() };

                // If the window size has changed, we need to resize the scene viewport
                if ( m_ViewPortWidth != viewPortDimensions.x || m_ViewPortHeight != viewPortDimensions.y ) {
                    m_ViewPortWidth = viewPortDimensions.x;
                    m_ViewPortHeight = viewPortDimensions.y;
                }

                // Index 0 is the final output
                if (m_CurrentPassSelectionIndex == 0) {
                    ImGui::Spacing();


                    // Reduce the output image to not take the whole window
                    m_ViewPortHeight *= 0.8f;
                    m_ViewPortWidth *= 0.8f;

                    m_ViewPortWidth = std::max( m_ViewPortWidth, m_ViewPortHeight / m_EditorMainCamera->GetAspectRatio() );

                    ImGui::Image( reinterpret_cast<ImTextureID>( m_ColorAttachmentDescriptorSet ),
                    ImVec2{ m_ViewPortWidth, m_ViewPortHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
                }

                ImGui::TreePop();
            }
        }

        auto OnUpdate() -> void override {
            constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding};

            ImGui::Spacing();
            if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "RenderPanelViewport_VkImpl::OnUpdate" ), styleFlags, "%s", "Scene")) {

                ImGui::Spacing();
                ImGui::Text( fmt::format( "Scene: {}", m_TargetScene->GetName() ).c_str() );

                ImGui::TreePop();
            }

            ImGui::Spacing();
            ShowPasses(styleFlags);

        }

    private:
        static auto GetDefaultSamplerCreateInfo() -> VkSamplerCreateInfo {
            VkSamplerCreateInfo samplerCreateInfo{ VulkanHelpers::Initializers::SamplerCreateInfo() };

            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

            samplerCreateInfo.maxAnisotropy = 1.0f;
            samplerCreateInfo.mipLodBias = 0.0f;
            samplerCreateInfo.minLod = 0.0f;
            samplerCreateInfo.maxLod = 1.0f;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

            return samplerCreateInfo;
        }

    private:
        static constexpr  std::array s_ImageCompositions{
            "Final output", "Color pass", "Shadow pass", "Depth pass"
        };

        Size_T m_CurrentPassSelectionIndex{ 0 };

        VkSampler m_ColorAttachmentSampler{};

        VkDescriptorSet m_ColorAttachmentDescriptorSet{};
    };

    static constexpr auto GetRendererPanel() -> std::string_view {
        return "Renderer";
    }

    RendererPanel::RendererPanel(const RendererPanelCreateInfo& createInfo)
        : Panel{ StringUtils::MakePanelName(ICON_MD_SETTINGS_DISPLAY, GetRendererPanel() ) },
        m_TargetScene{ createInfo.TargetScene } {

        // Initialize implementation
        RenderPanelViewport_VkImplCreateInfo sceneApiCreateInfo{
            .ViewportCreateInfo{
                .ViewportWidth{ createInfo.Width },
                .ViewportHeight{ createInfo.Height },
                .TargetScene{ createInfo.TargetScene },
                .Renderer{ createInfo.Renderer },
                .MainCamera{ createInfo.EditorMainCamera },
            },
        };

        // Set scene panel implementation
        m_Implementation = CreateScope<RenderPanelViewport_VkImpl>(sceneApiCreateInfo);

        if (m_Implementation != nullptr) {
            m_Implementation->Init();
        } else {
            MKT_APP_LOGGER_ERROR( "ScenePanel::ScenePanel - Failed to create Scene Panel ImGui implementation." );
        }
    }

    auto RendererPanel::OnUpdate( float timeStep ) -> void {
        if ( m_PanelIsVisible ) {
            static constexpr ImGuiWindowFlags windowFlags{
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar
            };

            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

            m_Implementation->OnUpdate();

            ImGui::End();
        }
    }

    RendererPanel::~RendererPanel() {
        m_Implementation = nullptr;
        m_TargetScene = nullptr;
    }
}// namespace Mikoto