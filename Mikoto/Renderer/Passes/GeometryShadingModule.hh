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

#ifndef MIKOTO_GEOMETRY_SHADING_MODULE_PASSES_HH
#define MIKOTO_GEOMETRY_SHADING_MODULE_PASSES_HH

#include <glm/glm.hpp>

#include <EASTL/fixed_vector.h>
#include <EASTL/fixed_hash_map.h>

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
        ePrefilterMap,// Blurred map (In shader mip 3 is used)
        eClearColor,
    };

    struct WireframeData {
        FGTextureHandle mColorImage{};
        FGPipelineHandle mDefaultPipeline{};
        FGPipelineHandle mOutputMergePipeline{};
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
        FGPipelineHandle mSkyboxProjectionPipeline_FlatImage{};
        FGPipelineHandle mSkyboxProjectionPipeline_Graphics{};

        FGPipelineHandle mBrdfPipeline{};
        FGPipelineHandle mIrradiancePipeline{};
        FGPipelineHandle mPrefilterPipeline{};

        FGSamplerHandle mSkyboxCubeSampler{};
        FGSamplerHandle mDefaultSampler{};

        core::f32 mExposure{};
        core::f32 mGamma{};

        // Tonemap
        FGTextureHandle mTonemapColor{};
        FGPipelineHandle mTonemapPipeline{};

        // Color gradient
        FGTextureHandle mColorGradientRenderTarget{};
        FGPipelineHandle mColorGradientPipeline{};

        // Box
        core::usize mBoxIndicesCount{};
        core::usize mBoxVerticesCount{};
        FGBufferHandle mBoxIndexBuffer{};
        FGBufferHandle mBoxVertexBuffer{};
    };

    class GeometryShadingModule {
    public:
        explicit GeometryShadingModule( rhi::RenderResolution resolution );

        auto SetClearColor( const rhi::Color& color ) -> void;
        auto SetResolution( rhi::RenderResolution resolution ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

        // Wireframe
        // Makes it so that our final composition image is merged with wireframe view
        auto SetMergeWireframeToFinalOutput( bool merge ) -> void;

        // SSAO
        auto SetEnableSsao( bool enable ) -> void;
        auto SetSsaoIntensity( core::f32 value ) -> void;

        // HDR
        auto SetGamma( core::f32 value ) -> void;
        auto SetExposure( core::f32 value ) -> void;
        auto SetAmbientScale( core::f32 ambient ) -> void;

        auto SetSkyboxMaterial( material::MaterialHandle material ) -> void;

        auto SetRenderBackground( SceneBackgroundType bg ) -> void;

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
        inline static const eastl::fixed_vector<core::float4x4, kMaxCubeFaces> kMatrices{
            // POSITIVE_X
            glm::rotate( glm::rotate( glm::mat4( 1.0f ), glm::radians( 90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) ), glm::radians( 180.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
            // NEGATIVE_X
            glm::rotate( glm::rotate( glm::mat4( 1.0f ), glm::radians( -90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) ), glm::radians( 180.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
            // POSITIVE_Y
            glm::rotate( glm::mat4( 1.0f ), glm::radians( -90.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
            // NEGATIVE_Y
            glm::rotate( glm::mat4( 1.0f ), glm::radians( 90.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
            // POSITIVE_Z
            glm::rotate( glm::mat4( 1.0f ), glm::radians( 180.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
            // NEGATIVE_Z
            glm::rotate( glm::mat4( 1.0f ), glm::radians( 180.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) ),
        };

        // This is used for debugging mainly so I can distinguish faces if texture sampling fails
        inline static const eastl::fixed_vector<rhi::Color, kMaxCubeFaces> kFaceColors{
            rhi::Color( 1.0f, 0.0f, 0.0f, 1.0f ),// +X : Red
            rhi::Color( 0.0f, 1.0f, 1.0f, 1.0f ),// -X : Cyan
            rhi::Color( 0.0f, 1.0f, 0.0f, 1.0f ),// +Y : Green (Vulkan Y points down)
            rhi::Color( 1.0f, 0.0f, 1.0f, 1.0f ),// -Y : Magenta
            rhi::Color( 0.0f, 0.0f, 1.0f, 1.0f ),// +Z : Blue
            rhi::Color( 1.0f, 1.0f, 0.0f, 1.0f ),// -Z : Yellow
        };

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryManagement{};

        rhi::Color mClearColor{ kColorBlue };

        static constexpr core::u32 kIrradianceDimensions{ 64 };
        static constexpr core::u32 kIrradianceMipLevels{ 1 };

        static constexpr core::u32 kPrefilterDimensions{ 1024 };
        core::u32 mPrefilterMipLevels{ as<core::u32>( math::Floor( math::Log2( kPrefilterDimensions ) ) ) + 1 };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        SceneBackgroundType mBackgroundType{ SceneBackgroundType::eSkybox };

        // SSAO
        bool mEnableSsao{ true };
        core::f32 mSsaoIntensity{ 1.0f };

        // IBL
        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };
        core::f32 mAbientScale{ 1.0f };
        material::MaterialHandle mSkyboxMaterial{};

        // Polygon complexity
        bool mMergeWireframeToFinalImage{};

        eastl::fixed_hash_map<material::SkyboxFace, FGTextureHandle, 6> mSkyboxFaces{};

        asset::ModelHandle mBoxModel{};
    };
}// namespace mikoto::renderer

#endif//MIKOTO_GEOMETRY_SHADING_MODULE_PASSES_HH
