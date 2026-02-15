//    Copyright 2025 ケイト
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

#ifndef MIKOTO_POST_EFFECTS_PASSES_HH
#define MIKOTO_POST_EFFECTS_PASSES_HH

#include <string_view>

#include <Assets/Font.hh>

#include <Library/Utility/Types.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Core/CommandContext.hh>

namespace Mikoto {

    class PostEffectsPass {
    public:
        explicit PostEffectsPass( RenderResolution resolution);

        auto SetScene(const Scene* scene) -> void;
        auto SetCamera(const Camera* camera) -> void;
        auto SetMeshCulling( MeshCulling &culling ) -> void;
        auto RegisterPasses(FrameGraph& graph, GpuDevice* device) -> void;

    private:
        auto RegisterTextRender( FrameGraph& graph, GpuDevice* device) -> void;
        auto RegisterTextRenderScatterWrites( FrameGraph& graph) -> void;
        auto RegisterObjectOutline( FrameGraph& graph, GpuDevice* device) -> void;
        auto RegisterSSAO( FrameGraph& graph ) -> void;
        auto RegisterBloom( FrameGraph& graph ) -> void;
        auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;

        auto TraverseTextList( CommandContext& ctx ) -> void;

        auto SetupTextForRender( CommandContext& context, const TransformComponent& transformComponent, const TextComponent& textComponent) -> void;

    private:
        struct TextRenderParams {
            Mat4F Proj{};
            Mat4F View{};
            Mat4F Model{};

            Vec4F OutlineColor{};
            float OutlineWidth{};

            Vec4F Position{};
            Vec4F Size{};
            Vec4F Color{};
            Vec2F TexCoords[4]{};
            UInt32 TexIndex{};
        };

        struct FontVertex {
            glm::vec3 Pos{};
            UInt32 TexIndex{};
        };

        struct InfiniteGridParameters {
            Mat4F CameraView{};
            Mat4F CameraProj{};
            Vec4F CameraPos{};
        };

        static constexpr UInt32 SSAO_KERNEL_SIZE{ 64 };

        struct SSAOParameters {
            Mat4F Projection{};
            std::array<Vec4F, SSAO_KERNEL_SIZE> Samples{};
        };

        static constexpr UInt32 MAX_GLYPHS{ 1024 * 1024 };

        std::array<FontVertex, 4> VERTICES{
            FontVertex{ { 0.0f, 0.0f, 0.0f }, 0 },
            FontVertex{ { 1.0f, 0.0f, 0.0f }, 1 },
            FontVertex{ { 1.0f, 1.0f, 0.0f }, 2 },
            FontVertex{ { 0.0f, 1.0f, 0.0f }, 3 }
        };

        std::array<UInt32, 6> INDICES{
            0, 1, 2,// first triangle
            2, 3, 0 // second triangle
        };

    private:
        BufferHandle m_TextVertexBuffer{};
        BufferHandle m_TextIndexBuffer{};

        const Scene* m_Scene{};
        const Camera* m_Camera{};

        Size m_GlyphCount{};
        std::vector<UInt32> m_TextInfoIndices{};
        std::vector<TextRenderParams> m_TextInfo{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };

        UInt32 m_SSOKernelSize{ 64 };
        std::vector<Vec4F> m_SSONoise{};
        SSAOParameters m_SSAOParameters{};
        SamplerHandle m_Sampler{};
        SamplerHandle m_SamplerNoise{};

        MeshCulling* m_MeshCullingPass{};

        InfiniteGridParameters m_InfiniteGridParameters{};
    };

}// namespace Mikoto

#endif // MIKOTO_POST_EFFECTS_PASSES_HH

