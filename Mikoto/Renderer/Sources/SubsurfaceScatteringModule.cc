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

// Pipeline reference (independent Slang/frame-graph implementation):
// https://github.com/iryoku/separable-sss
// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/
// https://github.com/bentoBAUX/Separable-Subsurface-Scattering-in-Unity-6
// https://iryoku.com/separable-sss/downloads/Separable-Subsurface-Scattering.pdf

#include <EASTL/array.h>

#include <Core/Blackboard.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/SubsurfaceScatteringModule.hh>

namespace mikoto::renderer {

    using namespace core;
    using namespace rhi;

    auto SubsurfaceScatteringModule::RegisterBlurPass( FrameGraph& graph, core::u32 axis,
        rhi::RenderResolution resolution ) -> void {

        const auto dimensions{ InferDimensions( resolution ) };
        const core::u32 width{ core::as<core::u32>( dimensions.first ) };
        const core::u32 height{ core::as<core::u32>( dimensions.second ) };
        const auto texture{ [&]( core::cstr name, renderer::rhi::Format format ) {
            return graph.Create( FGTextureDescription{}.SetName( name ).SetWidth( width ).SetHeight( height ).SetFormat( format ).SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopySource ) );
        } };
        auto& info{ graph.GetOrCreate<SubsurfaceScatteringInfo>() };
        const auto name{ axis == 0 ? "Subsurface_Horizontal" : "Subsurface_Vertical" };
        const auto output{ texture( name, rhi::Format::eRGBA16_FLOAT ) };
        info.mFiltered[axis] = output;
        const auto original{ info.mDiffuse };
        const auto metadata{ info.mIdentityDepth };
        const auto input{ axis == 0 ? original : info.mFiltered[0] };
        const auto materials{ graph.GetData<GeometryCullModuleInfo>().mMaterialsBuffer };
        const auto kernels{ graph.GetData<GeometryCullModuleInfo>().mSubsurfaceKernels };
        const auto makePipeline{ [&]( core::cstr name, core::cstr shader ) {
            return graph.Create( FGPipelineDescription{}.SetName( name ).SetPipelineType( renderer::rhi::PipelineType::eGraphics ).SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList ).SetCullMode( renderer::rhi::CullMode::eNone ).SetDepthTest( false ).SetDepthWrite( false ).SetBlendEnable( false ).AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT ).PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex ).PushShader( shader, FGStageType::ePixel ) );
        } };

        if ( axis == 0 ) {
            info.mBlurPipeline = makePipeline( "Subsurface_Blur", "SubsurfaceScattering_Frag.slang" );
        }

        const auto blurPipeline{ info.mBlurPipeline };
        const auto draw{ [=]( CommandContext& ctx, FGTextureHandle output, FGPipelineHandle pipeline ) {
            ctx.BeginRender( RenderBeginDescription{}.SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } ).AddRenderTarget( output, renderer::rhi::Color{ 0, 0, 0, 0 }, renderer::rhi::LoadOp::eClear ) );
            ctx.SetViewportState( renderer::rhi::ViewportState{}.AddViewportAndScissorRect( renderer::rhi::Viewport{ core::as<core::f32>( width ), core::as<core::f32>( height ) } ) );
            ctx.BindPipeline( pipeline );
            ctx.Draw( 3 );
            ctx.EndRender();
        } };
        graph.RegisterPass(
            name,
            FGPassType::eGraphics,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                // The original carries the material key even after the RGB-only first blur.
                builder.UseResource( original, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                if ( input.mHandle != original.mHandle ) {
                    builder.UseResource( input, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                builder.UseResource( metadata, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( materials, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( kernels, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( output, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [=]( CommandContext& ctx, core::Blackboard& ) {
                struct Parameters {
                    SPointer mMaterials;
                    SPointer mKernels;
                    core::u32 mOriginal;
                    core::u32 mInput;
                    core::u32 mMetadata;
                    core::u32 mAxis;
                    core::u32 mWidth;
                    core::u32 mHeight;
                } params{
                    .mMaterials = ctx.GetDeviceBufferAddress( materials ),
                    .mKernels = ctx.GetDeviceBufferAddress( kernels ),
                    .mOriginal = ctx.PushSRV( original ),
                    .mInput = ctx.PushSRV( input ),
                    .mMetadata = ctx.PushSRV( metadata ),
                    .mAxis = axis,
                    .mWidth = width,
                    .mHeight = height,
                };
                ctx.PushConstants( params );
                draw( ctx, output, blurPipeline );
            } );
    }

    auto SubsurfaceScatteringModule::RegisterCompositePass( FrameGraph& graph, GeometryCullModule& geometry,
        rhi::RenderResolution resolution, rhi::Multisampling multisampling ) -> void {

        const auto dimensions{ InferDimensions( resolution ) };
        const core::u32 width{ core::as<core::u32>( dimensions.first ) };
        const core::u32 height{ core::as<core::u32>( dimensions.second ) };
        const auto texture{ [&]( core::cstr name, renderer::rhi::Format format ) {
            return graph.Create( FGTextureDescription{}.SetName( name ).SetWidth( width ).SetHeight( height ).SetFormat( format ).SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopySource ) );
        } };
        const auto& data{ graph.GetData<SubsurfaceScatteringInfo>() };
        const auto original{ data.mDiffuse };
        const auto metadata{ data.mIdentityDepth };
        const auto specular{ data.mSpecular };
        const auto ambient{ data.mAmbient };
        const auto scene{ graph.GetData<GeomShadingModuleInfo>().mColorImage };
        const auto materials{ graph.GetData<GeometryCullModuleInfo>().mMaterialsBuffer };
        const auto vertical{ data.mFiltered[1] };
        const auto makePipeline{ [&]( core::cstr name, core::cstr shader ) {
            return graph.Create( FGPipelineDescription{}.SetName( name ).SetPipelineType( renderer::rhi::PipelineType::eGraphics ).SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList ).SetCullMode( renderer::rhi::CullMode::eNone ).SetDepthTest( false ).SetDepthWrite( false ).SetBlendEnable( false ).AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT ).PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex ).PushShader( shader, FGStageType::ePixel ) );
        } };
        const auto draw{ [=]( CommandContext& ctx, FGTextureHandle output, FGPipelineHandle pipeline ) {
            ctx.BeginRender( RenderBeginDescription{}.SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } ).AddRenderTarget( output, renderer::rhi::Color{ 0, 0, 0, 0 }, renderer::rhi::LoadOp::eClear ) );
            ctx.SetViewportState( renderer::rhi::ViewportState{}.AddViewportAndScissorRect( renderer::rhi::Viewport{ core::as<core::f32>( width ), core::as<core::f32>( height ) } ) );
            ctx.BindPipeline( pipeline );
            ctx.Draw( 3 );
            ctx.EndRender();
        } };

        if ( multisampling != renderer::rhi::Multisampling::eMsaaX1 ) {
            // Re-rasterize geometry to retain sample coverage and per-sample
            // occlusion. A fullscreen replacement would overwrite background
            // samples along silhouettes. No MSAA texture is sampled as Texture2D.
            const auto raster{ graph.GetData<GeomShadingModuleInfo>().mRasterColorImage };
            const auto depth{ graph.GetData<GeomShadingModuleInfo>().mRasterDepthImage };
            const auto camera{ graph.GetData<CameraModuleInfo>().mCameraData };
            const auto data{ graph.GetData<GeometryCullModuleInfo>() };
            const auto pipeline{ graph.Create( FGPipelineDescription{}.SetName( "Subsurface_Composite_MSAA" ).SetPipelineType( renderer::rhi::PipelineType::eGraphics ).SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList ).SetCullMode( renderer::rhi::CullMode::eNone ).SetDepthTest( true ).SetDepthWrite( false ).SetBlendEnable( false ).SetMultisampling( multisampling ).SetDepthFormat( renderer::rhi::Format::eD32 ).AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT ).PushShader( "SubsurfaceCompositeMSAA_Vert.slang", FGStageType::eVertex ).PushShader( "SubsurfaceCompositeMSAA_Frag.slang", FGStageType::ePixel ) ) };
            graph.RegisterPass(
                "Subsurface_Composite_MSAA",
                FGPassType::eGraphics,
                [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                    for ( auto handle: { original, vertical, specular, ambient, metadata } ) {
                        builder.UseResource( handle, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }

                    for ( auto handle: { camera, data.mGeometryBuffer, data.mInstanceBuffer, data.mDrawInstanceBuffer,
                              data.mGeometryAllocBuffer, data.mAttributeBuffer, data.mIndexBuffer,
                              data.mSkinningBuffer, data.mSkinningCacheBuffer } ) {
                        builder.UseResource( handle, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                    }

                    builder.UseResource( materials, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( data.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
                    builder.UseResource( depth, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                    builder.UseResource( raster, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },
                [=, &geometry]( CommandContext& ctx, core::Blackboard& ) {
                    struct Parameters {
                        SPointer mGeometry;
                        SPointer mSkinning;
                        SPointer mAllocations;
                        SPointer mCamera;
                        SPointer mMaterials;
                        core::u32 mOriginal;
                        core::u32 mFiltered;
                        core::u32 mSpecular;
                        core::u32 mAmbient;
                        core::u32 mMetadata;
                        core::u32 mSampler;
                    } params{
                        .mGeometry = ctx.GetDeviceBufferAddress( data.mGeometryBuffer ),
                        .mSkinning = ctx.GetDeviceBufferAddress( data.mSkinningBuffer ),
                        .mAllocations = ctx.GetDeviceBufferAddress( data.mGeometryAllocBuffer ),
                        .mCamera = ctx.GetDeviceBufferAddress( camera ),
                        .mMaterials = ctx.GetDeviceBufferAddress( materials ),
                        .mOriginal = ctx.PushSRV( original ),
                        .mFiltered = ctx.PushSRV( vertical ),
                        .mSpecular = ctx.PushSRV( specular ),
                        .mAmbient = ctx.PushSRV( ambient ),
                        .mMetadata = ctx.PushSRV( metadata ),
                        .mSampler = ctx.PushSampler( data.mMaterialSampler ),
                    };
                    ctx.PushConstants( params );
                    ctx.BeginRender( RenderBeginDescription{}.SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } ).AddRenderTarget( raster, renderer::rhi::Color{ 0, 0, 0, 0 }, renderer::rhi::LoadOp::eLoad ).AddDepthTarget( depth, renderer::rhi::LoadOp::eLoad ) );
                    ctx.SetViewportState( renderer::rhi::ViewportState{}.AddViewportAndScissorRect( renderer::rhi::Viewport{ core::as<core::f32>( width ), core::as<core::f32>( height ) } ) );
                    ctx.BindPipeline( pipeline );
                    geometry.DrawInstancesIndirect( ctx, GeometryDrawGroup::eOpaqueAndMasked );
                    ctx.EndRender();
                } );
            return;
        }

        const auto composite{ texture( "Subsurface_Composite", renderer::rhi::Format::eRGBA16_FLOAT ) };
        graph.GetOrCreate<SubsurfaceScatteringInfo>().mComposite = composite;
        const auto compositePipeline{ makePipeline( "Subsurface_Composite", "SubsurfaceComposite_Frag.slang" ) };
        graph.RegisterPass(
            "Subsurface_Composite",
            FGPassType::eGraphics,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                for ( auto handle: { original, vertical, specular, ambient, scene } ) {
                    builder.UseResource( handle, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                builder.UseResource( materials, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( composite, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [=]( CommandContext& ctx, core::Blackboard& ) {
                struct Parameters {
                    SPointer mMaterials;
                    core::u32 mOriginal;
                    core::u32 mFiltered;
                    core::u32 mSpecular;
                    core::u32 mAmbient;
                    core::u32 mScene;
                    core::u32 mPadding{};
                } params{
                    .mMaterials = ctx.GetDeviceBufferAddress( materials ),
                    .mOriginal = ctx.PushSRV( original ),
                    .mFiltered = ctx.PushSRV( vertical ),
                    .mSpecular = ctx.PushSRV( specular ),
                    .mAmbient = ctx.PushSRV( ambient ),
                    .mScene = ctx.PushSRV( scene ),
                };
                ctx.PushConstants( params );
                draw( ctx, composite, compositePipeline );
            } );
    }

    auto SubsurfaceScatteringModule::RegisterStorePass( FrameGraph& graph, rhi::RenderResolution resolution,
        rhi::Multisampling multisampling ) -> void {
        if ( multisampling != rhi::Multisampling::eMsaaX1 ) {
            return;
        }

        const auto dimensions{ InferDimensions( resolution ) };
        const core::u32 width{ core::as<core::u32>( dimensions.first ) };
        const core::u32 height{ core::as<core::u32>( dimensions.second ) };
        const auto composite{ graph.GetData<SubsurfaceScatteringInfo>().mComposite };
        const auto scene{ graph.GetData<GeomShadingModuleInfo>().mColorImage };
        // Read and write different images during composition; store only afterwards.
        graph.RegisterPass(
            "Subsurface_Store",
            FGPassType::eGraphics,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( composite, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( scene, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [=]( CommandContext& ctx, core::Blackboard& ) {
                const renderer::rhi::TextureSlice slice{ .mWidth = width, .mHeight = height };
                ctx.Copy( composite, slice, scene, slice );
            } );
    }

}// namespace mikoto::renderer
