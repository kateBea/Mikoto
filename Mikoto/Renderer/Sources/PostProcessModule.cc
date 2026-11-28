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

#include <cstdint>

#include <EASTL/array.h>
#include <EASTL/algorithm.h>
#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/TimeService.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/SceneCamera.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    static auto GetMaterialStage( 
        PostProcessModuleInfo& info, 
        material::PostProcessStage stage ) -> PostProcessMaterialStageInfo& {
        return stage == material::PostProcessStage::eBeforeToneMapping
            ? info.mBeforeToneMapping : info.mAfterToneMapping;
    }

    static auto GetFilmGrainAmount( 
        const Scene& scene, 
        const PostProcessMaterialComponent& component,
        const material::PostProcessMaterial& material ) -> f32 
    {
        if ( !material.IsFilmGrainEnabled() ) {
            return 0.0f;
        }

        const auto* entity{ scene.FindByID( component.GetCameraEntityID() ) };

        // An unassigned, deleted, or incompatible camera leaves the material inert.
        if ( !entity || !entity->HasComponent<CameraComponent>() ) {
            return 0.0f;
        }

        const auto& camera{ entity->GetComponent<CameraComponent>() };
        return camera.HasCamera() ? camera.GetCamera().GetFilmGrainSettings().mAmount : 0.0f;
    }

    static auto FindMaterialBloom( const Scene* scene ) -> const BloomSettings* {
        if ( !scene ) {
            return nullptr;
        }

        const BloomSettings* selected{};
        core::u64 selectedOwner{};

        for ( const auto [entity, tag, component]:
            scene->GetRegistry().view<TagComponent, PostProcessMaterialComponent>().each() ) {
            if ( !tag.IsActive() || ( selected && selectedOwner <= tag.GetGuid() ) ) {
                continue;
            }

            const auto handle{ component.GetMaterial() };
            const auto* material{ dynamic_cast<const material::PostProcessMaterial*>( handle.GetPtr() ) };

            // One HDR bloom chain serves the scene. Use the same entity order as grading.
            if ( material && material->IsBloomEnabled() ) {
                selected = &material->GetBloomSettings();
                selectedOwner = tag.GetGuid();
            }
        }

        return selected;
    }

    static auto GetChromaticOffsetPixels( const material::PostProcessMaterial& material ) -> core::float2 {
        if ( !material.IsChromaticAberrationEnabled() ) {
            return {};
        }

        const auto& settings{ material.GetChromaticAberrationSettings() };
        const auto radians{ glm::radians( settings.mDirection ) };
        const core::float2 direction{ glm::cos( radians ), glm::sin( radians ) };
        return direction * ( settings.mIntensity / glm::max( glm::abs( direction.x ), glm::abs( direction.y ) ) );
    }

    static auto CollectPostProcessMaterials( 
        const Scene* scene, 
        material::PostProcessStage stage,
        eastl::vector<PostProcessMaterialData>& materials, core::float2& chromaticOffsetPixels ) -> void
    {
        materials.clear();
        chromaticOffsetPixels = {};

        if ( !scene ) {
            return;
        }

        struct Item {
            const material::PostProcessMaterial* mMaterial{};
            core::u64 mEntity{};
            core::f32 mFilmGrainAmount{};
        };

        eastl::vector<Item> items{};

        for ( const auto [entity, tag, component]:
            scene->GetRegistry().view<scene::TagComponent, scene::PostProcessMaterialComponent>().each() ) {
            const auto handle{ component.GetMaterial() };
            const auto* material{ dynamic_cast<const material::PostProcessMaterial*>( handle.GetPtr() ) };

            if ( tag.IsActive() && material && material->GetStage() == stage ) {
                items.push_back( { material, tag.GetGuid(),
                    GetFilmGrainAmount( *scene, component, *material ) } );
            }
        }

        // Keep effect order stable when the registry rearranges its component storage.
        eastl::sort( items.begin(), items.end(),
            []( const Item& left, const Item& right ) {
            return left.mEntity < right.mEntity;
        } );

        // Every component applies its effect, including repeated material instances.
        for ( const auto& item: items ) {
            if ( materials.size() == PostEffectsPass::kMaxPostProcessMaterials ) {
                MKT_THROW_RUNTIME_ERROR( "Post-process material stage exceeds its 4096-material buffer capacity." );
            }

            // Sample the scene image once per stage, using stable entity order as with bloom.
            if ( chromaticOffsetPixels == core::float2{} ) {
                chromaticOffsetPixels = GetChromaticOffsetPixels( *item.mMaterial );
            }

            materials.push_back( { item.mMaterial->GetTintColor(), item.mMaterial->GetContrast(),
                item.mMaterial->GetSaturation(), core::as<core::u32>( stage ),
                item.mFilmGrainAmount } );
        }
    }

    PostEffectsPass::PostEffectsPass( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto PostEffectsPass::SetScene( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mScene = scene;
    }

    auto PostEffectsPass::SetCamera( const scene::Camera* camera ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mCamera = camera;
    }

    auto PostEffectsPass::SetPreviewMaterial( const material::PostProcessMaterial& material ) -> void {
        mPreviewMaterial = PostProcessMaterialData{
            .mTintColor = material.GetTintColor(),
            .mContrast = material.GetContrast(),
            .mSaturation = material.GetSaturation(),
            .mStage = core::as<core::u32>( material.GetStage() ),
            .mFilmGrainAmount = material.IsFilmGrainEnabled() ? scene::FilmGrainSettings{}.mAmount : 0.0f
        };

        mPreviewChromaticOffsetPixels = GetChromaticOffsetPixels( material );
        mBloomSettings = material.GetBloomSettings();
        mBloomSettings.mEnabled = material.IsBloomEnabled();
    }

    auto PostEffectsPass::SetEnableBloom( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mBloomSettings.mEnabled = value;
    }

    auto PostEffectsPass::SetGamma( core::f32 gamma ) -> void {
        MKT_BEGIN_PROFILER();
        mGamma = gamma;
    }

    auto PostEffectsPass::SetExposure( core::f32 exposure ) -> void {
        MKT_BEGIN_PROFILER();
        mExposure = exposure;
    }

    auto PostEffectsPass::SetInfiniteGridSettings( const InfiniteGridSettings &settings ) -> void {
        mInfiniteGrid = settings;
    }

    auto PostEffectsPass::RegisterInfiniteGrid( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "InfiniteGrid_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetDepthTest( true )
            .SetDepthWrite( false )
            .SetBlendEnable( true )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .PushShader( "InfiniteGrid_Vert.slang", FGStageType::eVertex )
            .PushShader( "InfiniteGrid_Frag.slang", FGStageType::ePixel ) };
        info.mInfiniteGridPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "InfiniteGrid",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };

                builder.UseResource( cameraData.mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prePassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mTonemapColor, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& cameraData{ b.Get<CameraModuleInfo>() };
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };
                const auto& postProcessData{ b.Get<PostProcessModuleInfo>() };

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( prePassData.mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( finalCompData.mTonemapColor, renderer::rhi::kColorGreen, renderer::rhi::LoadOp::eLoad ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    core::u32 mCameraBufferID{};
                    core::u32 mPadding[3]{};
                    core::float4 mMinorColor{ 0.45f, 0.45f, 0.45f, 0.35f };
                    core::float4 mMajorColor{ 0.55f, 0.55f, 0.55f, 0.55f };
                    core::float4 mXAxisColor{ 0.9f, 0.12f, 0.10f, 1.0f };
                    core::float4 mZAxisColor{ 0.12f, 0.35f, 0.95f, 1.0f };
                    core::float4 mWidths{};
                    core::float4 mYAxisColor{ 0.12f, 0.8f, 0.22f, 1.0f };
                    core::float4 mGrid{};// Cell size, opacity, vertical-axis draw, reserved.
                } params{
                    .mCameraBufferID = ctx.PushSRV( cameraData.mCameraData )
                };

                static_assert( sizeof( DrawParams ) == 128 );
                params.mWidths = { mInfiniteGrid.mLineWidth, mInfiniteGrid.mLineWidth * 1.3f,
                    mInfiniteGrid.mAxisWidth, mInfiniteGrid.mAxisWidth };
                params.mGrid = { mInfiniteGrid.mCellSize, mInfiniteGrid.mOpacity, 0.0f, 0.0f };
                params.mXAxisColor.a = mInfiniteGrid.mShowXAxis ? 1.0f : 0.0f;
                params.mZAxisColor.a = mInfiniteGrid.mShowZAxis ? 1.0f : 0.0f;

                ctx.PushConstants( params );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( postProcessData.mInfiniteGridPipeline );
                ctx.Draw( 3 );

                // The vertical axis has its own depth. A second draw in the same
                // pass lets scene geometry occlude it independently of the XZ plane.
                if ( mInfiniteGrid.mShowYAxis ) {
                    params.mGrid.z = 1.0f;
                    ctx.PushConstants( params );
                    ctx.Draw( 3 );
                }

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterSsao( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // https://learnopengl.com/Advanced-Lighting/SSAO
        // https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

        auto ssaoSamplerDesc{ FGSamplerDescription{}
            .SetName( "Ssao_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge )
            .SetBorderColor( renderer::rhi::kColorBlack ) };
        info.mSsaoSampler = graph.Create( ssaoSamplerDesc );

        const auto dimensions{ InferDimensions( mResolution ) };
        const auto colorImage{ FGTextureDescription{}
            .SetName( "Ssao_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eR8_UNORM ) };
        info.mSsaoColorTarget = graph.Create( colorImage );

        const auto blurColorImage{ FGTextureDescription{}
            .SetName( "SsaoBlur_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eR8_UNORM ) };
        info.mSsaoBlurColorTarget = graph.Create( blurColorImage );

        const auto noiseTexture{ FGTextureDescription{}
            .SetName( "SSAO_NoiseImage01" )
            .SetWidth( core::as<core::i32>( kSsaoNoiseDimensions ) )
            .SetHeight( core::as<core::i32>( kSsaoNoiseDimensions ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                renderer::rhi::TextureUsageFlagsBits::ShaderResource | 
                renderer::rhi::TextureUsageFlagsBits::CopyDest )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT ) };
        info.mSsaoNoiseTexture = graph.Create( noiseTexture );

        const auto kernelBufferDesc{ FGBufferDescription{}
            .SetName( "SSAO_KernelBuffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | 
                renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( MKT_VECTOR_SIZE_BYTES( mSsaoKernelSamples ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        info.mSsaoKernelBuffer = graph.Create( kernelBufferDesc );

        mSsaoNoiseData.clear();

        // Generate noise
        for ( core::u32 i{}; i < kSsaoNoiseDimensions * kSsaoNoiseDimensions; i++ ) {
            // rotate around z-axis (in tangent space)
            mSsaoNoiseData.emplace_back(
                math::random::GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f,
                math::random::GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, 0.0f, 0.0f );
        }

        // Generate samples
        for ( core::u32 i{}; i < kSsaoKernelSize; ++i ) {
            core::float4 sample{
                core::as<core::f32>( math::random::GetRandomReal( 0.0, 1.0 ) ) * 2.0 - 1.0,
                core::as<core::f32>( math::random::GetRandomReal( 0.0, 1.0 ) ) * 2.0 - 1.0,
                core::as<core::f32>( math::random::GetRandomReal( 0.0, 1.0 ) ), 0.0
            };

            sample = glm::normalize( sample );
            sample *= math::random::GetRandomReal( 0.0, 1.0 );

            core::f32 scale{ core::as<core::f32>( i ) / 64.0f };

            // scale samples s.t. they're more aligned to center of kernel
            scale = core::as<core::f32>( math::Lerp( 0.1f, 1.0f, scale * scale ) );
            sample *= scale;

            mSsaoKernelSamples[i] = sample;
        }

        // Pass to upload Data for SSAO
        graph.RegisterPass<PostProcessModuleInfo>(
            "SSAO_DataUpload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& b, PostProcessModuleInfo& data ) {
                b.UseResource( data.mSsaoNoiseTexture, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mSsaoKernelBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<PostProcessModuleInfo>() };

                ctx.Copy( data.mSsaoNoiseTexture, mSsaoNoiseData.data(), MKT_VECTOR_SIZE_BYTES( mSsaoNoiseData ) );
                ctx.Copy( data.mSsaoKernelBuffer, 0, mSsaoKernelSamples.data(), MKT_VECTOR_SIZE_BYTES( mSsaoKernelSamples ) );
            } );

        graph.SetExecutionPolicy( "SSAO_DataUpload", FGExecutionPolicy::eOnWake );

        // AO is visibility data, not translucent color. Both shaders output
        // only red, so alpha blending would depend on an unwritten alpha
        // component. Replace each texel directly; white means unoccluded.
        const auto ssaoRenderPipelineDesc{ FGPipelineDescription{}
            .SetName( "SsaoRender_Pipeline01" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eR8_UNORM )
            .SetBlendEnable( false )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "SSAO_Frag.slang", FGStageType::ePixel ) };
        info.mSsaoRenderPipeline = graph.Create( ssaoRenderPipelineDesc );

        graph.RegisterPass(
            "Ssao_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& ssaoData{ blackboard.Get<PostProcessModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };

                builder.UseResource( blackboard.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( ssaoData.mSsaoColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( ssaoData.mSsaoNoiseTexture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( ssaoData.mSsaoKernelBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( prePassData.mGBufferPositionTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prePassData.mGBufferNormalTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& ssaoData{ blackboard.Get<PostProcessModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };

                const auto renderDimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( renderDimensions.first ), core::as<core::i32>( renderDimensions.second ) } )
                    .AddRenderTarget( ssaoData.mSsaoColorTarget, renderer::rhi::kColorWhite, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    core::u32 mSamplerID{};

                    core::u32 mKernelBufferID{};
                    core::u32 mNoiseTextureID{};
                    core::u32 mGBufferNormalsID{};
                    core::u32 mGBufferPositionsID{};

                    core::u32 mCameraBufferID{};
                } params{
                    .mSamplerID = ctx.PushSampler( ssaoData.mSsaoSampler ),

                    .mKernelBufferID = ctx.PushSRV( ssaoData.mSsaoKernelBuffer ),
                    .mNoiseTextureID = ctx.PushSRV( ssaoData.mSsaoNoiseTexture ),
                    .mGBufferNormalsID = ctx.PushSRV( prePassData.mGBufferNormalTarget ),
                    .mGBufferPositionsID = ctx.PushSRV( prePassData.mGBufferPositionTarget ),

                    .mCameraBufferID = ctx.PushSRV( blackboard.Get<CameraModuleInfo>().mCameraData )
                };

                ctx.PushConstants( params );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( renderDimensions.first ), core::as<core::f32>( renderDimensions.second ) ) ) );

                ctx.BindPipeline( ssaoData.mSsaoRenderPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );

        const auto ssaoBlurPipelineDesc{ FGPipelineDescription{}
            .SetName( "SsaoBlur_Pipeline01" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eR8_UNORM )
            .SetBlendEnable( false )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "SSAOBlur_Frag.slang", FGStageType::ePixel ) };
        info.mSsaoBlurPipeline = graph.Create( ssaoBlurPipelineDesc );

        graph.RegisterPass(
            "Ssao_Blur",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, core::Blackboard& blackboard ) {
                const auto& ssaoData{ blackboard.Get<PostProcessModuleInfo>() };

                b.UseResource( blackboard.Get<PrepassModuleInfo>().mGBufferPositionTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( ssaoData.mSsaoColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( ssaoData.mSsaoBlurColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& ssaoData{ blackboard.Get<PostProcessModuleInfo>() };

                const auto renderDimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( renderDimensions.first ), core::as<core::i32>( renderDimensions.second ) } )
                    .AddRenderTarget( ssaoData.mSsaoBlurColorTarget, renderer::rhi::kColorWhite, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    core::u32 mSamplerID{};
                    core::u32 mSsaoColorTargetID{};
                    core::u32 mPositionID{};
                } params{
                    .mSamplerID = ctx.PushSampler( ssaoData.mSsaoSampler ),
                    .mSsaoColorTargetID = ctx.PushSRV( ssaoData.mSsaoColorTarget ),
                    .mPositionID = ctx.PushSRV( blackboard.Get<PrepassModuleInfo>().mGBufferPositionTarget )
                };
                ctx.PushConstants( params );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( renderDimensions.first ), core::as<core::f32>( renderDimensions.second ) ) ) );

                ctx.BindPipeline( ssaoData.mSsaoBlurPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterBloom( FrameGraph& graph, FGTextureHandle source,
        FGTextureHandle materialSource ) -> void {
        // https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };
        info.mBloomSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Bloom_LinearClamp" )
            .SetFilter( renderer::rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge ) );

        info.mBloomDownSamplePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "BloomDownSample_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .PushShader( "BloomDown_Vert.slang", FGStageType::eVertex )
            .PushShader( "BloomDown_Frag.slang", FGStageType::ePixel ) );

        info.mBloomUpSamplePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "BloomUpSample_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .PushShader( "BloomUp_Vert.slang", FGStageType::eVertex )
            .PushShader( "BloomUp_Frag.slang", FGStageType::ePixel ) );

        auto dimensions{ InferDimensions( mResolution ) };
        core::u32 width{ core::as<core::u32>( dimensions.first ) };
        core::u32 height{ core::as<core::u32>( dimensions.second ) };
        info.mBloomChainImages.clear();

        for ( core::u32 mip{}; mip < kMaxBloomChainImages; ++mip ) {
            width = eastl::max( 1u, width / 2 );
            height = eastl::max( 1u, height / 2 );

            auto image{ FGTextureDescription{}
                .SetName( string::Format( "Bloom Down Mip {}", mip ) )
                .SetWidth( width )
                .SetHeight( height )
                .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) };
            const auto down{ graph.Create( image ) };

            // The smallest level is already reconstructed. All other levels
            // combine their original detail with the lower-resolution result.
            const auto reconstructed{ mip + 1 == kMaxBloomChainImages ? down : graph.Create( image.SetName( string::Format( "Bloom Reconstructed Mip {}", mip ) ) ) };

            info.mBloomChainImages.emplace_back( PostProcessModuleInfo::ImageDescription{
                .mWidth = width,
                .mHeight = height,
                .mImage = down,
                .mReconstructed = reconstructed } );
        }

        info.mBloomResult = info.mBloomChainImages.front().mReconstructed;

        for ( core::u32 mip{}; mip < kMaxBloomChainImages; ++mip ) {
            graph.RegisterPass(
                string::Format( "BloomDownSample Mip {}", mip ),
                FGPassType::eGraphics,
                [mip, source, materialSource]( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                    const auto& post{ blackboard.Get<PostProcessModuleInfo>() };
                    const auto input{ mip == 0 ? source : post.mBloomChainImages[mip - 1].mImage };
                    builder.UseResource( input, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    if ( mip == 0 && materialSource.mHandle ) {
                        builder.UseResource( materialSource, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }

                    builder.UseResource( post.mBloomChainImages[mip].mImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },
                [this, mip, source, materialSource]( CommandContext& ctx, core::Blackboard& blackboard ) {
                    auto& post{ blackboard.Get<PostProcessModuleInfo>() };
                    auto input{ mip == 0 ? source : post.mBloomChainImages[mip - 1].mImage };

                    // Snapshot the settings and source together before running the filter chain.
                    if ( mip == 0 ) {
                        const auto* materialBloom{ FindMaterialBloom( mScene ) };
                        post.mBloomSettings = materialBloom ? *materialBloom : mBloomSettings;

                        if ( materialBloom && materialSource.mHandle ) {
                            input = materialSource;
                        }
                    }

                    if ( !post.mBloomSettings.mEnabled || post.mBloomSettings.mIntensity == 0.0f ) {
                        return;
                    }

                    const auto& target{ post.mBloomChainImages[mip] };
                    struct DrawParams {
                        core::u32 mSamplerID;
                        core::u32 mTargetImageID;
                        core::f32 mThreshold;
                        core::u32 mPrefilter;
                    } params{
                        .mSamplerID = ctx.PushSampler( post.mBloomSampler ),
                        .mTargetImageID = ctx.PushSRV( input ),
                        .mThreshold = post.mBloomSettings.mThreshold,
                        .mPrefilter = mip == 0 ? 1u : 0u,
                    };

                    ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( target.mWidth ), core::as<core::i32>( target.mHeight ) } )
                        .AddRenderTarget( target.mImage, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear ) );

                    ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( target.mWidth ), core::as<core::f32>( target.mHeight ) ) ) );

                    ctx.PushConstants( params );

                    ctx.BindPipeline( post.mBloomDownSamplePipeline );
                    ctx.Draw( 3 );
                    ctx.EndRender();
                } );
        }
        
        for ( core::i32 mip{ core::as<core::i32>( kMaxBloomChainImages ) - 2 }; mip >= 0; --mip ) {
            graph.RegisterPass(
                string::Format( "BloomUpSample Mip {} to {}", mip + 1, mip ),
                FGPassType::eGraphics,
                [mip]( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                    const auto& post{ blackboard.Get<PostProcessModuleInfo>() };
                    builder.UseResource( post.mBloomChainImages[mip + 1].mReconstructed, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( post.mBloomChainImages[mip].mImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( post.mBloomChainImages[mip].mReconstructed, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },
                [mip]( CommandContext& ctx, core::Blackboard& blackboard ) {
                    const auto& post{ blackboard.Get<PostProcessModuleInfo>() };

                    if ( !post.mBloomSettings.mEnabled || post.mBloomSettings.mIntensity == 0.0f ) {
                        return;
                    }

                    const auto& target{ post.mBloomChainImages[mip] };
                    struct DrawParams {
                        core::u32 mSamplerID{};
                        core::u32 mLowerImageID{};
                        core::u32 mDetailImageID{};
                        core::f32 mScatter{};
                        core::f32 mRadius{};
                    } params{
                        .mSamplerID = ctx.PushSampler( post.mBloomSampler ),
                        .mLowerImageID = ctx.PushSRV( post.mBloomChainImages[mip + 1].mReconstructed ),
                        .mDetailImageID = ctx.PushSRV( target.mImage ),
                        .mScatter = post.mBloomSettings.mScatter,
                        .mRadius = post.mBloomSettings.mRadius,
                    };

                    ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( target.mWidth ), core::as<core::i32>( target.mHeight ) } )
                        .AddRenderTarget( target.mReconstructed, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear ) );

                    ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( target.mWidth ), core::as<core::f32>( target.mHeight ) ) ) );

                    ctx.PushConstants( params );

                    ctx.BindPipeline( post.mBloomUpSamplePipeline );
                    ctx.Draw( 3 );
                    ctx.EndRender();
                } );
        }
    }
    
    auto PostEffectsPass::RegisterPostProcess( FrameGraph& graph, material::PostProcessStage stage, FGTextureHandle source ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( stage != material::PostProcessStage::eBeforeToneMapping && stage != material::PostProcessStage::eAfterToneMapping ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid post-process material stage." );
        }

        const bool before{ stage == material::PostProcessStage::eBeforeToneMapping };
        const core::cstr prefix{ before ? "PostProcessMaterials_BeforeToneMapping" : "PostProcessMaterials_AfterToneMapping" };

        const auto& geometry{ graph.GetOrCreate<GeomShadingModuleInfo>() };
        const auto& effects{ graph.GetOrCreate<DisplayEffectsModuleInfo>() };
        const bool fixedSource{ source.mHandle != FGResourceManager::kInvalidResourceHandle };

        if ( !fixedSource ) {
            source = before ? effects.mDepthOfFieldColor : geometry.mTonemapColor;
        }

        auto& info{ GetMaterialStage( graph.GetOrCreate<PostProcessModuleInfo>(), stage ) };
        const auto dimensions{ InferDimensions( mResolution ) };

        info.mMaterials = graph.Create( FGBufferDescription{}
            .SetName( string::Format( "{}_Materials", prefix ) )
            .SetSizeBytes( kMaxPostProcessMaterials * sizeof( PostProcessMaterialData ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest ) );

        info.mColorImage = graph.Create( FGTextureDescription{}
            .SetName( before ? "PostProcessMaterial_ColorImage01" : "PostProcessMaterial_ColorImage02" )
            .SetWidth( dimensions.first )
            .SetHeight( dimensions.second )
            .SetDimensions( rhi::TextureDimension::eTexture2D )
            .SetFormat( rhi::Format::eRGBA16_FLOAT )
            .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource ) );

        info.mSampler = graph.Create( FGSamplerDescription{}
            .SetName( string::Format( "{}_Sampler", prefix ) )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) );

        info.mPipeline = graph.Create( FGPipelineDescription{}
            .SetName( string::Format( "{}_Pipeline", prefix ) )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eRGBA16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "PostProcessMaterial_Frag.slang", FGStageType::ePixel ) );

        // Select active component owners, filter their materials by stage,
        // and upload their packed records. The graph transitions the buffer.
        graph.RegisterPass(
            string::Format( "{}_Filter", prefix ),
            FGPassType::eTransfer,
            [stage]( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( GetMaterialStage( b.Get<PostProcessModuleInfo>(), stage ).mMaterials,
                    FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, stage]( CommandContext& ctx, core::Blackboard& b ) {
                SetupPostProcessMaterials( ctx, b, stage );
            } );

        // Compose materials in order so each application consumes the previous color.
        // Copy the source even with no materials to keep removal and stage changes immediate.
        graph.RegisterPass(
            string::Format( "{}_Graphics", prefix ),
            FGPassType::eGraphics,
            [stage, source, before, fixedSource]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& info{ GetMaterialStage( b.Get<PostProcessModuleInfo>(), stage ) };
                builder.UseResource( source, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( info.mMaterials, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( info.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                if ( !before && !fixedSource ) {
                    const auto& effects{ b.Get<DisplayEffectsModuleInfo>() };
                    // Camera effects can be toggled without rebuilding the graph.
                    builder.UseResource( effects.mChromaAbRenderTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( effects.mVignetteCompositeTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
            },
            [this, stage, source, before, fixedSource]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& info{ GetMaterialStage( b.Get<PostProcessModuleInfo>(), stage ) };
                const auto& effects{ b.Get<DisplayEffectsModuleInfo>() };
                const auto dimensions{ InferDimensions( mResolution ) };

                auto input{ source };

                if ( !before && !fixedSource ) {
                    if ( effects.mVignetteEnabled ) {
                        input = effects.mVignetteCompositeTarget;
                    }
                    else if ( effects.mChromaticAberrationEnabled ) {
                        input = effects.mChromaAbRenderTarget;
                    }
                }

                struct DrawParams {
                    core::u64 mMaterials{};
                    core::u32 mSceneImage{};
                    core::u32 mSampler{};
                    core::u32 mMaterialCount{};
                    core::u32 mStage{};
                    core::f32 mElapsedSeconds{};
                    core::u32 mPadding{};
                    core::float2 mChromaticOffsetPixels{};
                } parameters{
                    .mMaterials = ctx.GetDeviceBufferAddress( info.mMaterials ),
                    .mSceneImage = ctx.PushSRV( input ),
                    .mSampler = ctx.PushSampler( info.mSampler ),
                    .mMaterialCount = effects.mVisualizeDepthOfField ? 0u : info.mMaterialCount,
                    .mStage = core::as<core::u32>( stage ),
                    .mElapsedSeconds = core::as<core::f32>( TimeService::Get()->GetTime() ),
                    .mChromaticOffsetPixels = effects.mVisualizeDepthOfField ? core::float2{} : info.mChromaticOffsetPixels
                };

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ core::as<core::i32>( dimensions.first ),
                            core::as<core::i32>( dimensions.second ) } )
                    .AddRenderTarget( info.mColorImage, rhi::kColorTransparent, rhi::LoadOp::eClear ) );

                ctx.BindPipeline( info.mPipeline );
                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect(
                    rhi::Viewport{ core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) } ) );
                ctx.PushConstants( parameters );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::SetupPostProcessMaterials( CommandContext& ctx, core::Blackboard& b, material::PostProcessStage stage ) -> void {
        MKT_BEGIN_PROFILER();

        auto& info{ GetMaterialStage( b.Get<PostProcessModuleInfo>(), stage ) };

        if ( mPreviewMaterial ) {
            mPostProcessMaterials.clear();
            info.mChromaticOffsetPixels = {};

            if ( mPreviewMaterial->mStage == core::as<core::u32>( stage ) ) {
                mPostProcessMaterials.push_back( *mPreviewMaterial );
                info.mChromaticOffsetPixels = mPreviewChromaticOffsetPixels;
            }
        }
        else {
            CollectPostProcessMaterials( mScene, stage, mPostProcessMaterials, info.mChromaticOffsetPixels );
        }

        info.mMaterialCount = core::as<core::u32>( mPostProcessMaterials.size() );

        if ( !mPostProcessMaterials.empty() ) {
            ctx.Copy( info.mMaterials, 0, mPostProcessMaterials.data(), MKT_VECTOR_SIZE_BYTES( mPostProcessMaterials ) );
        }
    }

    auto PostEffectsPass::RegisterEyeAdaptationPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // https://alextardif.com/HistogramLuminance.html
        graph.RegisterPass(
            "EyeAdaptation_Luminance",
            FGPassType::eCompute,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );

        graph.RegisterPass(
            "EyeAdaptation",
            FGPassType::eCompute,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto PostEffectsPass::GetBloomSettings() -> BloomSettings& {
        MKT_BEGIN_PROFILER();
        return mBloomSettings;
    }

}// namespace mikoto::renderer
