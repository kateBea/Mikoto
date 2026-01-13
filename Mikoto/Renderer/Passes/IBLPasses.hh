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

#ifndef MIKOTO_IBL_PASSES_HH
#define MIKOTO_IBL_PASSES_HH

#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>
#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Assets//Texture.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Library/Data/ResourcePool.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    // These passes are supposed to be run once
    // IBL passes are not frame work (run every frame).
    // Maybe we should extend the or handle in in a way to specify a set of passes that need to be ran before anything else
    // Future passes that depend on them might need to be delayed until data is ready

    class EnvCubePass final : public FramePass {
    public:
        explicit EnvCubePass()
           : FramePass{ "EnvCubePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class IrradiancePass final : public FramePass {
    public:
        explicit IrradiancePass()
           : FramePass{ "IrradiancePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class PrefilterPass final : public FramePass {
    public:
        explicit PrefilterPass()
           : FramePass{ "PrefilterPass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class BRDFLutPass final : public FramePass {
    public:
        explicit BRDFLutPass()
           : FramePass{ "BRDFLutPass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class SkyboxPass final : public FramePass {
    public:
        explicit SkyboxPass()
            : FramePass{ "SkyboxPass", FramePassType::RENDER } {}

        auto Setup( FrameGraphBuilder& builder ) -> void override;
        auto Execute( PassCommandList& commandList ) -> void override;

        auto SetCamera( const Camera* camera ) -> void;
        auto SetCubeMap( TextureHandle cubeMap ) -> void;

    private:
        struct SkyboxUBO {
            Mat4F View{};
            Mat4F Projection{};
        };

    private:
        SamplerHandle m_Sampler{};

        SkyboxUBO m_SkyboxUBO{};
        TextureHandle m_CubeMap{};
    };

    class ShadingPass final : public FramePass {
    public:
        explicit ShadingPass()
            : FramePass{ "ShadingPass", FramePassType::RENDER } {}

        auto Setup( FrameGraphBuilder& device ) -> void override;
        auto Execute( PassCommandList& commandList ) -> void override;

        auto SetScene( Scene* scene ) -> void;
        auto SetCamera( const Camera* camera ) -> void;

        auto EnableSkybox( bool enable ) -> void;
        auto SetClearColor( const Vec4F& vec ) -> void;

    private:
        auto UploadInstanceData( PassCommandList& commandList ) -> void;
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
        };

    private:
        ShaderLightListParams m_LightsInfo{};
        ShaderCameraParams m_FrameUBO{};

        bool m_UseSkybox{ false };

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        std::vector<ShaderMaterialParams> m_Meshes{};
        ankerl::unordered_dense::map<MeshNode*, MeshInstanceInfo> m_MeshDrawState{};
    };
}


#endif//MIKOTO_IBL_PASSES_HH
