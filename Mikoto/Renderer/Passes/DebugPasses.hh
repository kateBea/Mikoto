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

#ifndef MIKOTO_DEBUG_PASSES_HH
#define MIKOTO_DEBUG_PASSES_HH

#include <string_view>

#include <Scene/Scene.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    class ObjectOutlinePass final : public FramePass {
    public:
        explicit ObjectOutlinePass()
            : FramePass{ "ObjectOutlinePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    class WireFramePass final : public FramePass {
    public:
        explicit WireFramePass()
            : FramePass{ "WireFramePass", FramePassType::RENDER } {}

        auto Setup( FrameGraphBuilder& device ) -> void override;
        auto Execute( PassCommandList& commandList ) -> void override;

        auto SetScene( Scene* scene ) -> void;
        auto SetClearColor( const Vec4F& vec ) -> void;

        auto ShowColorImage( bool value ) -> void;

    private:
        auto DrawObjects( PassCommandList& commandList ) -> void;
        auto TraverseMeshList( PassCommandList& commandList ) -> void;

    public:
        struct MeshInstanceInfo {
            DrawIndexedState InstanceDrawState{};
            ankerl::unordered_dense::map<UInt64, bool> ActiveEntities{};
            ankerl::unordered_dense::map<UInt64, ShaderMaterialParams> InstanceInfos{};

            MKT_NODISCARD auto IsActive( UInt64 entityID ) const -> bool {
                bool result{ false };
                const auto it{ ActiveEntities.find( entityID ) };

                if ( it != ActiveEntities.end() ) {
                    result = it->second;
                }

                return result;
            }

            auto Disable(UInt64 entityID )-> void {
                const auto it{ ActiveEntities.find( entityID ) };

                if ( it != ActiveEntities.end() ) {
                    it->second = false;
                }
            }
        };

    private:

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        std::vector<ShaderMaterialParams> m_Meshes{};
        ankerl::unordered_dense::map<MeshNode*, MeshInstanceInfo> m_MeshDrawState{};

        bool m_ShowColor{ false };
    };

    // Material Pass that renders a sphere with a texture on a sphere
    // Uses IBL precomputed info
    class MaterialPreviewPass final : public FramePass {
    public:
        explicit MaterialPreviewPass()
            : FramePass{ "MaterialPreviewPass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    class TextPass final : public FramePass {
    public:
        explicit TextPass()
            : FramePass{ "TextPass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        Scene* m_Scene{};
    };

    // This class is kept for debug purposes
    // just computes prime numbers
    class SimpleComputePass final : public FramePass {
    public:
        explicit SimpleComputePass()
            : FramePass{ "SimpleComputePass", FramePassType::COMPUTE } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;


    };

    // Simple pass for testing purposes
    // A triangle with interpolation
    class HelloTrianglePass final : public FramePass {
    public:
        explicit HelloTrianglePass()
            : FramePass{ "HelloTrianglePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    // Simple pass for testing purposes
    // Displays a texture
    class HelloTexture final : public FramePass {
    public:
        explicit HelloTexture()
            : FramePass{ "HelloTexture", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

    private:

        struct HelloTextureUniformBuffer {
            Int32 TextureIndex{ SRGTextures::INVALID_TEXTURE_INDEX };
        };
    };

    // Simple pass for testing purposes
    // A colored/textured cube
    class HelloCubePass final : public FramePass {
    public:
        explicit HelloCubePass()
            : FramePass{ "HelloTrianglePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

}

#endif //MIKOTO_DEBUG_PASSES_HH