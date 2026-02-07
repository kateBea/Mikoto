//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_RENDERER_PANEL_HH
#define MIKOTO_RENDERER_PANEL_HH

#include <Panels/Panel.hh>

namespace Mikoto {
    struct EditorState;

    struct RendererPanelCreateInfo {
        EditorState* State{};
    };

    enum class FinalCompositionTarget {
        COLOR,
        NORMALS,
        POSITION,
        FINAL_IMAGE,
        ENUM_MAX,
    };

    class RendererPanel final : public Panel {
    public:
        explicit RendererPanel(const RendererPanelCreateInfo& info);

        auto OnUpdate(float timeStep) -> void override;

        MKT_NODISCARD auto IsVsyncEnabled() const -> bool;

        MKT_NODISCARD auto EnableSkyboxLDR() const -> bool;

        ~RendererPanel() override = default;

    private:

        auto DrawPassInfo() -> void;
        auto DrawRendererConfig() -> void;

        auto DrawSSAOSettings() -> void;

        auto DrawIBLSettings() -> void;
        auto DrawShadowMappingSettings() -> void;

    private:

        EditorState* m_EditorState{};

        bool m_EnableSkyboxLDR{ false };
        bool m_ShowPassGraph{ false };
        bool m_IsWireframeEnabled{ false };

        bool m_EnableVSync{ false };

        // SSAO
        bool m_EnableSSAO{ true };
        UInt32 m_SSAODimensions{ 8 }; // m_SSAODimensions * m_SSAODimensions
        float m_KernelSize{ 0.5f };
        float m_SSAORadius{ 0.5f };
        float m_SSAOBias{ 0.5f };
        float m_SSAOStrength{ 1.5f };

        FinalCompositionTarget m_FinalCompositionTarget{ FinalCompositionTarget::FINAL_IMAGE };
    };
}


#endif//MIKOTO_RENDERER_PANEL_HH
