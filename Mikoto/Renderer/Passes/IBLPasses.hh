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

        auto SetScene( Scene* scene) -> void;
        auto SetCamera( const Camera *camera ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetClearColor( const Vec4F& color ) -> void;

        auto SetResolution( RenderResolution resolution) -> void;

        auto SetEquirectangularMap(TextureHandle texture2D) -> void;

        auto SetUsePrecomputedLDRCubeMap(bool value) -> void;
        auto SetPrecomputedLDRCubeMap( TextureHandle cubeMap ) -> void;

        auto SetExposure( float value ) -> void;
        auto SetGamma( float value ) -> void;
        auto EnableSkybox(bool enable) -> void;

        auto SetMeshCulling(MeshCulling& cullingPass) -> void;

    private:
        auto RegisterSkyboxRender( FrameGraph& graph ) -> void;
        auto RegisterIrradiance( FrameGraph& graph ) -> void;
        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterBRDFLut( FrameGraph& graph ) -> void;
        auto RegisterSkybox( FrameGraph& graph ) -> void;
        auto RegisterShading( FrameGraph& graph ) -> void;
        auto RegisterDirShadowMap( FrameGraph& graph ) -> void;
        auto RegisterPointShadowMap( FrameGraph& graph ) -> void;
        auto RegisterSpotShadowMap( FrameGraph& graph ) -> void;
        auto RegisterDebugViewsPass( FrameGraph& graph ) -> void;

    private:
        inline static const std::vector<glm::mat4> s_Matrices{
            // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
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

        struct SkyboxUBO {
            Mat4F View{};
            Mat4F Projection{};
            float Exposure{};
            float Gamma{};
        };

        struct SkyboxRenderParams {
            Mat4F MVP{};
        };

        struct DirectionalShadowMapCameraInfo {
            Mat4F LightView{};
            Mat4F LightProjection{};
        };

    private:

        static constexpr UInt32 MAX_MIP_LEVELS{ 7 };

    private:
        // Skybox
        SkyboxUBO m_SkyboxUBO{};
        TextureHandle m_CubeMap{};
        bool m_UseSkybox{ false };

        UInt32 m_IrradianceMipLevels{ 1 };
        UInt32 m_PrefilterMipLevels{};

        UInt32 m_IrradianceDimensions{ 64 };
        UInt32 m_PrefilterDimensions{ 512 };

        PrefilterParameters m_PrefilterParameters{};

        IrradianceParameters m_IrradianceParameters{};

        TextureHandle m_Skybox2D{};
        bool m_RequestUpdateSkybox{ false };
        bool m_UsePrecomputedLDRCubeMap{ false };
        SkyboxRenderParams m_SkyboxRenderParameters{};

        SamplerHandle m_CubeMapSampler{};
        SamplerHandle m_BRDFLutSampler{};
        SamplerHandle m_Skybox2DSampler{};

        ShaderLightListParams m_LightsInfo{};
        ShaderCameraParams m_FrameUBO{};

        DirectionalShadowMapCameraInfo m_DirectionalShadowMapCameraInfo{};

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        MeshCulling* m_MeshCullingPass{};

        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };
    };
}

#endif//MIKOTO_IBL_PASSES_HH
