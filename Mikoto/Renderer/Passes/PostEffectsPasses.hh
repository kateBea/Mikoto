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
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace Mikoto {

    class PostEffectsPass {
    public:
        explicit PostEffectsPass( RenderResolution resolution);

        auto SetScene(const Scene* scene) -> void;
        auto SetCamera(const Camera* camera) -> void;
        auto RegisterPasses(FrameGraph& graph, GpuDevice* device) -> void;

    private:
        auto RegisterTextRender( FrameGraph& graph, GpuDevice* device) -> void;
        auto RegisterObjectOutline( FrameGraph& graph, GpuDevice* device) -> void;
        auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;

        auto TraverseTextList( CommandContext& commandList ) -> void;

        auto SetupRenderParams( CommandContext& context ) -> void;
        auto SetupTextForRender( FontHandle font, const Camera* camera, Vec4F position, std::string_view text, double fontSize, Vec4F color, CommandContext& commandList ) -> void;

    private:
        struct alignas( 16 ) TextRenderParams {
            Mat4F Proj{};
            Mat4F View{};

            Vec4F Position{};
            Vec4F Size{};
            Vec4F Color{};
            Vec2F TexCoords[4]{};
            UInt32 TexIndex{};
        };

        struct alignas( 16 ) TextParamsUBO {
            Vec4F OutlineColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            float OutlineWidth{ 2.0f };
        };

        struct FontVertex {
            glm::vec3 Pos{};
            UInt32 TexIndex{};
        };

        struct alignas( 16 ) InfiniteGridParameters {
            Mat4F CameraView{};
            Mat4F CameraProj{};
            Vec4F CameraPos{};
        };

        static constexpr UInt32 MAX_STRING{ 8096 * 10 };

        std::vector<TextRenderParams> m_TextRenderParams{};

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
        TextParamsUBO m_TextRenderUBO{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };

        InfiniteGridParameters m_InfiniteGridParameters{};
    };

}// namespace Mikoto

#endif // MIKOTO_POST_EFFECTS_PASSES_HH

