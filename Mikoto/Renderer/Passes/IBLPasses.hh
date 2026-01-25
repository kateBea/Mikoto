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

#include <glm/glm.hpp>

#include <Scene/Scene.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    class IBLPasses {
    public:
        explicit IBLPasses( RenderResolution resolution);

        auto SetScene( Scene* scene) -> void;
        auto SetCamera( const Camera *camera ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetResolution( RenderResolution resolution) -> void;

        auto SetCubeMap( TextureHandle cubeMap ) -> void;
        auto SetExposure( float value ) -> void;
        auto SetGamma( float value ) -> void;
        auto EnableSkybox(bool enable) -> void;

    private:
        auto RegisterIrradiance( FrameGraph& graph ) -> void;
        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterBRDFLut( FrameGraph& graph ) -> void;
        auto RegisterSkybox( FrameGraph& graph ) -> void;
        auto RegisterShading( FrameGraph& graph ) -> void;

    private:
        inline static const std::vector<glm::mat4> s_Matrices{
            // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        };

    private:
        struct alignas(16) IrradianceParameters {
            float DeltaPhi{};
            float DeltaTheta{};
        };

        struct alignas(16) IrradianceCamInfo {
            Mat4F MVP{};
        };

        struct alignas(16) PrefilterParameters {
            float Roughness{};
            UInt32 NumSamples{};
        };

        struct alignas(16) PrefilterCamInfo {
            Mat4F MVP{};
        };

        struct alignas(16) SkyboxUBO {
            Mat4F View{};
            Mat4F Projection{};
            float Exposure{};
            float Gamma{};
        };

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

        static constexpr UInt32 MAX_MIP_LEVELS{ 7 };

    private:

        auto UploadInstanceData( CommandContext& context ) -> void;
        auto TraverseMeshList( CommandContext& context ) -> void;

    private:
        // Skybox
        SkyboxUBO m_SkyboxUBO{};
        TextureHandle m_CubeMap{};
        bool m_UseSkybox{ false };

        UInt32 m_IrradianceMipLevels{};
        UInt32 m_PrefilterMipLevels{};

        UInt32 m_IrradianceDimensions{ 64 };
        UInt32 m_PrefilterDimensions{ 512 };

        PrefilterParameters m_PrefilterParameters{};
        PrefilterCamInfo m_PrefilterCameraInfo{};

        IrradianceCamInfo m_CameraInfo{};
        IrradianceParameters m_Parameters{};

        SamplerHandle m_CubeMapSampler{};

        ShaderLightListParams m_LightsInfo{};
        ShaderCameraParams m_FrameUBO{};

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        std::vector<ShaderMaterialParams> m_Meshes{};
        ankerl::unordered_dense::map<MeshNode*, MeshInstanceInfo> m_MeshDrawState{};

        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };
    };
}

#endif//MIKOTO_IBL_PASSES_HH
