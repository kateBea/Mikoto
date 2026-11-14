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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_GEOMETRY_SHADING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_GEOMETRY_SHADING_MODULE_HH

#include <bit>

#include <EASTL/fixed_vector.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/AssetModel.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    // TODO: study how to account for bump mapping etc
    // I need it to properly render the ancient rune stones mesh from sketch-fab
    // https://youtu.be/cM7RjEtZGHw

    // https://alextardif.com/arealights.html
    // https://catlikecoding.com/unity/tutorials/procedural-grid/
    // https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-18-using-vertex-texture-displacement

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
        FGTextureHandle mRasterColorImage{};
        FGTextureHandle mRasterDepthImage{};
        FGTextureHandle mSkyboxCubeRT{};
        FGTextureHandle mAtmosphereCube{};
        FGTextureHandle mPrefilterCubeRT{};
        FGTextureHandle mAtmospherePrefilter{};
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

        auto RegisterResolve( FrameGraph& graph, bool transparent = false ) -> void;

        auto RegisterSkyboxGeometryUpload( FrameGraph& graph ) -> void;
        auto RegisterBrdfLut( FrameGraph& graph ) -> void;
        auto RegisterSkyboxProjection( FrameGraph& graph ) -> void;

        /**
         * @brief Generates averaged source-cubemap mips used by GGX prefiltering.
         * @param graph Graph receiving one render pass and one copy pass per mip.
         * @details Reduces bright HDR sampling dots baked into reflections. These
         * source mips are separate from the prefilter's roughness-dependent output.
         * Each pass batches all six faces; a 2048 cubemap requires 22 passes.
         * Register after skybox projection and before prefiltering.
         */
        auto RegisterSkyboxMipmaps( FrameGraph& graph ) -> void;

        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterIrradiance( FrameGraph& graph ) -> void;
        auto RegisterSkyboxRender( FrameGraph& graph ) -> void;
        auto RegisterShading( FrameGraph& graph, bool transparent = false, bool subsurface = false, bool contour = false ) -> void;

        auto SetMultisampling( rhi::Multisampling samples ) -> void;

        // Not Opaque Passes
        auto RegisterTransparentPass( FrameGraph& graph ) -> void;
        auto RegisterContours( FrameGraph& graph ) -> void;

        /** @brief Supplies isolated skin diffuse lighting to the subsurface module. */
        auto RegisterSubsurfaceLighting( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

        /** @brief Selects environment target capacity before graph registration.
         * Missing environments use tiny placeholders so bindings remain valid.
         */
        auto SetEnvironmentAllocated( bool allocated ) -> void;

        auto SetClearColor( const rhi::Color& color ) -> void;
        auto SetResolution( rhi::RenderResolution resolution ) -> void;

        // Wireframe
        // Makes it so that our final composition image is merged with wireframe view
        auto SetMergeWireframeToFinalOutput( bool merge ) -> void;

        // Register after SSS composition/MSAA resolve so overlays are not overwritten.
        auto RegisterWireframe( FrameGraph& graph ) -> void;

        // SSAO
        auto SetEnableSsao( bool enable ) -> void;
        auto SetSsaoIntensity( core::f32 value ) -> void;

        MKT_NODISCARD auto IsSsaoEnabled() const -> bool;
        MKT_NODISCARD auto GetSsaoIntensity() const -> core::f32;

        // HDR
        auto SetGamma( core::f32 value ) -> void;
        auto SetSkyboxBlur( core::f32 blur ) -> void;
        auto SetExposure( core::f32 value ) -> void;
        auto SetAmbientScale( core::f32 ambient ) -> void;

        auto SetSkyboxMaterial( material::MaterialHandle material ) -> void;
        auto SetSkyboxPassesEnabled( FrameGraph& graph, bool enabled ) -> void;
        auto SetEnvironmentLightingEnabled( bool enabled ) -> void;
        auto SetAtmosphereEnabled( bool enabled ) -> void;

        auto SetRenderBackground( SceneBackgroundType bg ) -> void;

    private:
        // https://www.sctheblog.com/blog/vulkan-frame/
        // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
        // https://docs.vulkan.org/spec/latest/chapters/textures.html#_cube_map_face_selection
        // https://johannesugb.github.io/gpu-programming/setting-up-a-proper-vulkan-projection-matrix/
        inline static const eastl::fixed_vector<core::float4x4, rhi::kMaxCubeFaces> kMatrices{
            // +X
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 1, 0, 0 ), glm::vec3( 0, -1, 0 ) ),
            // -X
            glm::lookAt( glm::vec3( 0 ), glm::vec3( -1, 0, 0 ), glm::vec3( 0, -1, 0 ) ),
            // +Y
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, -1, 0 ), glm::vec3( 0, 0, -1 ) ),
            // -Y
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, 1, 0 ), glm::vec3( 0, 0, 1 ) ),
            // +Z
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, 0, 1 ), glm::vec3( 0, -1, 0 ) ),
            // -Z
            glm::lookAt( glm::vec3( 0 ), glm::vec3( 0, 0, -1 ), glm::vec3( 0, -1, 0 ) ),
        };

        // This is used for debugging mainly so I can distinguish faces if texture sampling fails
        inline static const eastl::fixed_vector<rhi::Color, rhi::kMaxCubeFaces> kFaceColors{
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

        rhi::Color mClearColor{ renderer::rhi::kColorBlue };

        static constexpr core::u32 kIrradianceDimensions{ 64 };
        static constexpr core::u32 kIrradianceMipLevels{ 1 };

        core::u32 mPrefilterDimensions{ 1 };
        core::u32 mSkyboxDimensions{ 1 };
        core::u32 mSkyboxMipLevels{ 1 };
        core::u32 mPrefilterMipLevels{ core::as<core::u32>( math::Floor( math::Log2( mPrefilterDimensions ) ) ) + 1 };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        SceneBackgroundType mBackgroundType{ SceneBackgroundType::eSkybox };

        // SSAO
        bool mEnableSsao{ true };
        core::f32 mSsaoIntensity{ 1.0f };

        // IBL
        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };
        core::f32 mAbientScale{ 1.0f };
        core::f32 mSkyboxBlur{ .05f };
        material::MaterialHandle mSkyboxMaterial{};
        bool mEnvironmentLightingEnabled{ true };
        bool mAtmosphereEnabled{};
        bool mSkyboxPassesDisabled{};

        // Polygon complexity
        bool mMergeWireframeToFinalImage{};

        asset::ModelHandle mBoxModel{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_GEOMETRY_SHADING_MODULE_HH

