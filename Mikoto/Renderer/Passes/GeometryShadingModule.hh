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

#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Assets/Model.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    // TODO: study how to account for bump mapping etc
    // I need it to properly render the ancient rune stones mesh from sketch-fab
    // https://youtu.be/cM7RjEtZGHw

    enum class SceneBackgroundType {
        eSkybox,
        ePrefilterMap, // Blurred map (In shader mip 3 is used)
        eClearColor,
    };

    struct WireframeData {
        FGTextureHandle mColorImage{};
        FGPipelineHandle mPipeline{};
    };

    struct GeomShadingModuleInfo {
        FGTextureHandle mBrdfColorTarget{};

        // Final composition image
        FGTextureHandle mColorImage{};

        // IBL environment textures
        FGTextureHandle mSkyboxCubeRT{};
        FGTextureHandle mPrefilterCubeRT{};
        FGTextureHandle mIrradianceCubeRT{};

        FGPipelineHandle mShadingPipeline{};
        FGPipelineHandle mSkyboxRenderPipeline{};
        FGPipelineHandle mSkyboxProjectionPipeline{};

        FGPipelineHandle mBrdfPipeline{};
        FGPipelineHandle mIrradiancePipeline{};
        FGPipelineHandle mPrefilterPipeline{};

        FGSamplerHandle mSkyboxCubeSampler{};
        FGSamplerHandle mDefaultSampler{};

        f32 mExposure{};
        f32 mGamma{};

        // Tonemap
        FGTextureHandle mTonemapColor{};
        FGPipelineHandle mTonemapPipeline{};

        // Box
        size_t mBoxIndicesCount{};
        size_t mBoxVerticesCount{};
        FGBufferHandle mBoxIndexBuffer{};
        FGBufferHandle mBoxVertexBuffer{};
    };

    class GeometryShadingModule {
    public:
        explicit GeometryShadingModule( RenderResolution resolution);

        auto SetClearColor( const Color& color ) -> void;
        auto SetResolution( RenderResolution resolution) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera *camera ) -> void;
        auto SetGeometryManager( GeometryCullModule& geom) -> void;

        // Wireframe
        // Makes it so that our final composition image is merged with wireframe view
        auto SetMergeWireframeToFinalOutput( bool merge ) -> void;
        auto SetEnableWireframe( bool enable ) -> void;

        // SSAO
        auto SetEnableSsao( bool enable ) -> void;
        auto SetSsaoIntensity( float value ) -> void;

        // HDR
        auto SetGamma( float value ) -> void;
        auto SetExposure( float value ) -> void;
        auto SetAmbientScale( f32 ambient ) -> void;

        // This guy will be deprecated as we
        // will use skybox materials instead
        auto SetEquirectangular(FGTextureHandle texture) -> void;

        auto SetSkyboxMaterial(material::MaterialHandle material) -> void;

        auto SetRenderBackground(SceneBackgroundType bg) -> void;

    private:
        auto RegisterShading( FrameGraph& graph ) -> void;

        auto RegisterWireframe( FrameGraph& graph ) -> void;

        auto RegisterSkyboxRender( FrameGraph& graph ) -> void;
        auto RegisterSkyboxProjection( FrameGraph& graph ) -> void;

        auto RegisterBrdfLut( FrameGraph& graph ) -> void;
        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterIrradiance( FrameGraph& graph ) -> void;

        auto PrepareGeometryShadingAssets( FrameGraph& graph ) -> void;

    private:
        inline static const eastl::fixed_vector<float4x4, kMaxCubeFaces> kMatrices{
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

        // This is used for debugging mainly so I can distinguish faces if texture sampling fails
        inline static const eastl::fixed_vector<Color, kMaxCubeFaces> kFaceColors{
            Color( 1.0f, 0.0f, 0.0f, 1.0f ), // +X : Red
            Color( 0.0f, 1.0f, 1.0f, 1.0f ), // -X : Cyan
            Color( 0.0f, 1.0f, 0.0f, 1.0f ), // +Y : Green (Vulkan Y points down)
            Color( 1.0f, 0.0f, 1.0f, 1.0f ), // -Y : Magenta
            Color( 0.0f, 0.0f, 1.0f, 1.0f ), // +Z : Blue
            Color( 1.0f, 1.0f, 0.0f, 1.0f ), // -Z : Yellow
        };

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryManagement{};

        Color mClearColor{ kColorBlue };

        static constexpr u32 kIrradianceDimensions{ 64 };
        static constexpr u32 kIrradianceMipLevels{ 1 };

        static constexpr u32 kPrefilterDimensions{ 1024 };
        u32 mPrefilterMipLevels{ as<u32>( math::Floor( math::Log2( kPrefilterDimensions ) ) ) + 1 };

        RenderResolution mResolution{ RenderResolution::e1080P };
        SceneBackgroundType mBackgroundType{ SceneBackgroundType::eClearColor };

        // SSAO
        bool mEnableSsao{ true };
        f32 mSsaoIntensity{ 1.0f };

        // IBL
        f32 mGamma{ 1.0f };
        f32 mExposure{ 1.0f };
        f32 mAbientScale{ 1.0f };
        rhi::TextureHandle mSkyboxHdrTexture{};
        FGTextureHandle mEquirectangularTexture{};
        material::MaterialHandle mSkyboxMaterial{};

        asset::ModelHandle mBoxModel{};
    };
}

#endif//MIKOTO_IBL_PASSES_HH
