//
// Created by kate on 11/30/25.
//

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiUtility.hh>

#include <Layers/EditorLayer.hh>
#include <Panels/PassVisualizerPanel.hh>

#include "ImGui/ImGuiService.hh"

namespace Mikoto {

    static auto DrawTexturePreview(TextureHandle composition, float maxWidth, float maxHeight) -> void {
        // No flipping, the final image is already in the correct viewport coordinates
        // In the case of vulkan this is also taken into account when setting up the viewport
        ImTextureID id{ ImGuiService::Get()->GetTextureID( composition) };
        if (!id) {
            return;
        }

        // Query texture size
        ImVec2 texSize{ static_cast<float>( composition->GetWidth() ), static_cast<float>( composition->GetHeight() ) };
        if (texSize.x == 0 || texSize.y == 0) {
            return;
        }

        const float aspect{ texSize.x / texSize.y };
        ImVec2 displaySize{};

        if (texSize.x > texSize.y) {
            displaySize.x = std::min(maxWidth, texSize.x);
            displaySize.y = displaySize.x / aspect;

            if (displaySize.y > maxHeight) {
                displaySize.y = maxHeight;
                displaySize.x = displaySize.y * aspect;
            }
        } else {
            displaySize.y = std::min(maxHeight, texSize.y);
            displaySize.x = displaySize.y * aspect;

            if (displaySize.x > maxWidth) {
                displaySize.x = maxWidth;
                displaySize.y = displaySize.x / aspect;
            }
        }

        ImGui::Image(id, displaySize);
    }

    PassVisualizerPanel::PassVisualizerPanel( const PassVisualizerDescription &description )
        : Panel{ "Preview" }, m_EditorState{ description.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( fmt::format("{}", ICON_MD_WINDOW), m_PanelName );
    }

    auto PassVisualizerPanel::OnUpdate( float ) -> void {
        if ( !m_PanelIsVisible ) {
            return;
        }

        constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };

        // Expand scene view to window bounds (no padding)
        ImGuiUtils::ImGuiScopedStyleVar winPad( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

        m_PanelIsFocused = ImGui::IsWindowFocused();
        m_PanelIsHovered = ImGui::IsWindowHovered();

        if (m_EditorState && !m_EditorState->PassesCompositions.empty()) {
            ImGui::BeginChild("PassList", ImVec2(150, 0), true);
            for ( auto &name: m_EditorState->PassesCompositions | std::views::keys ) {
                if (ImGui::Selectable(name.c_str(), name == m_ActivePassName)) {
                    m_ActivePassName = name;
                }
            }

            ImGui::EndChild();
            ImGui::SameLine();

            // Main preview
            ImGui::BeginGroup();
            if (!m_ActivePassName.empty()) {
                auto it = m_EditorState->PassesCompositions.find(m_ActivePassName);
                if (it != m_EditorState->PassesCompositions.end()) {
                    ImVec2 availSize = ImGui::GetContentRegionAvail();
                    DrawTexturePreview(it->second, availSize.x, availSize.y);
                }
            } else {
                ImGui::TextDisabled("Select a pass to preview...");
            }
            ImGui::EndGroup();
        } else {
            ImGui::TextDisabled("No passes available for preview.");
        }

        ImGui::End();
    }
}// namespace Mikoto
