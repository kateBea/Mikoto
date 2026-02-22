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

#ifndef MIKOTO_IBL_PASSES_HH
#define MIKOTO_IBL_PASSES_HH

#include <glm/glm.hpp>

#include <Scene/Scene.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    class IBLPasses {
    public:
        explicit IBLPasses( RenderResolution resolution);

        auto SetClearColor( const Vec4F& color ) -> void;
        auto SetResolution( RenderResolution resolution) -> void;
        auto RegisterPasses( FrameGraph& graph, GpuDevice* device ) -> void;

        auto SetScene( Scene* scene ) -> void;
        auto SetCamera( const Camera *camera ) -> void;
        auto SetMeshCulling(MeshCulling& cullingPass) -> void;

        // HDR
        auto SetEquirectangularMap(TextureHandle texture2D) -> void;

        // LDR
        auto UseCubeMap(bool value) -> void;
        auto SetCubeMap( TextureHandle cubeMap ) -> void;

        MKT_NODISCARD auto IsUsingCubeMap() const -> bool;

        // IBL
        auto SetExposure( float value ) -> void;
        auto SetGamma( float value ) -> void;
        auto EnableSkybox(bool enable) -> void;
        auto SetMaxReflectionLOD( float value ) -> void;
        auto UseConvolutedCube(bool enable)-> void;

        MKT_NODISCARD auto IsUsingConvolutedCube() const -> bool;

    private:
        auto RegisterSkyboxRender( FrameGraph& graph ) -> void;
        auto RegisterIrradiance( FrameGraph& graph ) -> void;
        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterBRDFLut( FrameGraph& graph ) -> void;
        auto RegisterSkybox( FrameGraph& graph ) -> void;
        auto RegisterShading( FrameGraph& graph ) -> void;
        auto RegisterMetalRoughnessPBR( FrameGraph& graph ) -> void;
        auto RegisterDirShadowMap( FrameGraph& graph ) -> void;
        auto RegisterPointShadowMap( FrameGraph& graph ) -> void;
        auto RegisterSpotShadowMap( FrameGraph& graph ) -> void;
        auto RegisterDebugViewsPass( FrameGraph& graph ) -> void;

    private:
        inline static const std::vector<glm::mat4> s_Matrices{
            // +X
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 1, 0, 0 ), glm::vec3( 0, -1, 0 ) ),
            // -X
            glm::lookAt( glm::vec3( 0 ), glm::vec3( -1, 0, 0 ), glm::vec3( 0, -1, 0 ) ),
            // +Y
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, 1, 0 ), glm::vec3( 0, 0, 1 ) ),
            // -Y
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, -1, 0 ), glm::vec3( 0, 0, -1 ) ),
            // +Z
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, 0, 1 ), glm::vec3( 0, -1, 0 ) ),
            // -Z
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, 0, -1 ), glm::vec3( 0, -1, 0 ) ),
        };

    private:

        struct IrradiancePassData {
            bool Update{ false };
        };

        struct IrradianceParameters {
            Mat4F MVP{};
            float DeltaPhi{};
            float DeltaTheta{};
        };

        struct PrefilterPassData {
            bool Update{ false };
        };

        struct PrefilterParameters {
            Mat4F MVP{};
            float Roughness{};
            UInt32 NumSamples{};
        };

        struct IBLParameters {
            float Exposure{};
            float Gamma{};

            float MaxReflectionLOD{ 9.0 };
            float MaxMipLevel{ 1.0 };

            Int32 IsSkyboxActive{ MKT_SHADER_FALSE };
        };

        struct SkyboxRenderParams {
            Mat4F MVP{};
        };

        struct LightCameraInfo {
            Mat4F LightView{};
            Mat4F LightProjection{};
        };

        static constexpr UInt32 MAX_MIP_LEVELS{ 7 };

    private:
        // Skybox
        IBLParameters m_IBLParameters{};
        bool m_UseSkybox{ false };

        UInt32 m_IrradianceMipLevels{ 1 };
        UInt32 m_PrefilterMipLevels{};

        UInt32 m_IrradianceDimensions{ 64 };
        UInt32 m_PrefilterDimensions{ 1024 };

        PrefilterParameters m_PrefilterParameters{};

        IrradianceParameters m_IrradianceParameters{};

        ModelHandle m_BoxModel{};
        DrawIndexedState m_DrawBoxIndexedState{};

        TextureHandle m_CubeMap{};
        TextureHandle m_Equirectangular{};

        bool m_RequestUpdateSkybox{ false };
        bool m_UseCubeMap{ false };
        SkyboxRenderParams m_SkyboxRenderParameters{};

        bool m_UseConvolutedCubeMap{ false };

        SamplerHandle m_CubeMapSampler{};
        SamplerHandle m_BRDFLutSampler{};
        SamplerHandle m_Skybox2DSampler{};

        ShaderLightListParams m_LightsInfo{};

        LightCameraInfo m_DirectionalShadowMapCameraInfo{};

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        MeshCulling* m_MeshCullingPass{};

        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };
    };
}

#endif//MIKOTO_IBL_PASSES_HH
