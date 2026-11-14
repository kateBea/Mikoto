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

#include <stdexcept>

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <Core/Exception.hh>

#include <Assets/Importer.hh>

#include <Material/SkyboxMaterial.hh>
#include <Material/PostProcessMaterial.hh>

#include <Animation/Utility.hh>
#include <Animation/Animator.hh>

#include <Renderer/Passes/ThumbnailPasses.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

    ThumbnailPasses::ThumbnailPasses( asset::ModelHandle model, core::u32 width, core::u32 height )
        : mModel{ eastl::move( model ) }, mWidth{ width }, mHeight{ height } {

        if ( !mModel || !width || !height || width > 4096 || height > 4096 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid GPU preview model or dimensions." );
        }

        bool valid{};
        core::float3 minimum{};
        core::float3 maximum{};

        // A local animator evaluates Ozz's rest pose without registering with
        // AnimationSystem or starting any clips. Skeleton-only models work too.
        animation::Animator restPose{ mModel };

        for ( core::usize index{}; index < mModel->GetMeshNodeCount(); ++index ) {
            const auto& mesh{ mModel->GetMeshNode( index ) };

            if ( !mesh.GetVertexBuffer() || !mesh.GetIndexBuffer() || mesh.GetCollisionIndices().empty() ) {
                continue;
            }

            mMeshes.push_back( { &mesh, {}, {}, core::Ref<material::SurfaceMaterial>::New( mesh.GetProperties() ) } );
            auto& previewMesh{ mMeshes.back() };
            previewMesh.mPaletteOffset = mPalettes.size();
            const auto& joints{ restPose.GetJointModelMatrices( mesh.GetSkinBinding().mRigIndex ) };
            const core::usize count{ mesh.GetSkinBinding().mJoints.size() };

            if ( count && !joints.empty() && !mesh.GetSkinningVertices().empty() ) {
                mPalettes.resize( mPalettes.size() + count );

                if ( animation::BuildSkinningPalette( mesh.GetSkinBinding(), { joints.data(), joints.size() },
                         { mPalettes.data() + previewMesh.mPaletteOffset, count } ) ) {
                    previewMesh.mJointCount = static_cast<core::u32>( count );
                }
                else {
                    mPalettes.resize( previewMesh.mPaletteOffset );
                }
            }

            core::float3 meshMinimum{};
            core::float3 meshMaximum{};
            bool meshValid{};
            const core::usize vertexCount{ previewMesh.mJointCount ? mesh.GetSkinningVertices().size() : mesh.GetCollisionVertices().size() };

            for ( core::usize vertexIndex{}; vertexIndex < vertexCount; ++vertexIndex ) {
                const core::float3 vertex{ previewMesh.mJointCount
                                               ? animation::SkinVertexPosition( mesh.GetSkinningVertices()[vertexIndex], { mPalettes.data() + previewMesh.mPaletteOffset, count } )
                                               : mesh.GetCollisionVertices()[vertexIndex] };
                const core::float3 point{ mesh.GetTransform() * core::float4{ vertex, 1.0f } };

                if ( !meshValid ) {
                    meshMinimum = meshMaximum = point;
                    meshValid = true;
                }
                else {
                    meshMinimum = glm::min( meshMinimum, point );
                    meshMaximum = glm::max( meshMaximum, point );
                }

                if ( !valid ) {
                    minimum = maximum = point;
                    valid = true;
                }
                else {
                    minimum = glm::min( minimum, point );
                    maximum = glm::max( maximum, point );
                }
            }

            previewMesh.mCenter = ( meshMinimum + meshMaximum ) * 0.5f;
        }

        if ( !valid || mMeshes.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Model has no renderable GPU preview geometry." );
        }

        const core::float3 extent{ maximum - minimum };
        const core::f32 scale{ 2.0f / eastl::max( eastl::max( extent.x, extent.y ), eastl::max( extent.z, 0.001f ) ) };
        const core::float3 origin{ ( minimum.x + maximum.x ) * 0.5f, minimum.y, ( minimum.z + maximum.z ) * 0.5f };
        // Normalize once at import boundaries. Orbit and lighting then work in
        // stable preview units, regardless of the source model's units or pivot.
        mNormalization = glm::scale( core::float4x4{ 1.0f }, core::float3{ scale } ) * glm::translate( core::float4x4{ 1.0f }, -origin );
        mNormalizedHalfExtent = extent * ( scale * 0.5f );
        mDrawData.resize( mMeshes.size() + 1 );

        if ( mPalettes.empty() ) {
            mPalettes.push_back( core::float4x4{ 1.0f } );
        }
    }

    auto ThumbnailPasses::RegisterUpload( FrameGraph& graph ) -> void {
        mPaletteBuffer = graph.Create( FGBufferDescription{}
            .SetName( "Preview.RestPose" )
            .SetSizeBytes( mPalettes.size() * sizeof( core::float4x4 ) )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );
        mSceneBuffer = graph.Create( FGBufferDescription{}
            .SetName( "Preview.Scene" )
            .SetSizeBytes( sizeof( SceneData ) )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );
        mDrawBuffer = graph.Create( FGBufferDescription{}
            .SetName( "Preview.Draws" )
            .SetSizeBytes( mDrawData.size() * sizeof( DrawData ) )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        for ( auto& mesh: mMeshes ) {
            mesh.mVertices = graph.ImportBuffer( mesh.mMesh->GetVertexBuffer() );
            mesh.mIndices = graph.ImportBuffer( mesh.mMesh->GetIndexBuffer() );
        }

        graph.RegisterPass(
            "Preview.Upload",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mSceneBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mDrawBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mPaletteBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                Upload( context );
            } );
    }

    auto ThumbnailPasses::RegisterShadows( FrameGraph& graph ) -> void {
        mShadow = graph.Create( FGTextureDescription{}
            .SetName( "Preview.Shadow" )
            .SetWidth( 1024 )
            .SetHeight( 1024 )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetFormat( renderer::rhi::Format::eD32 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
        mSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Preview.Repeat" )
            .SetMipLevels( 16.0f )
            .SetFilter( renderer::rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat ) );
        mShadowPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Preview.Shadow" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetBlendEnable( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .PushShader( "ThumbnailMesh_Vert.slang", FGStageType::eVertex )
            .PushShader( "ThumbnailShadow_Frag.slang", FGStageType::ePixel ) );
        graph.RegisterPass(
            "Preview.Shadows",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                DeclareGeometryReads( builder );
                builder.UseResource( mShadow, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                context.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ 1024, 1024 } )
                    .AddDepthTarget( mShadow ) );
                SetViewport( context, 1024, 1024 );

                if ( mSettings.mShadows ) {
                    DrawMeshes( context, true );
                }

                context.EndRender();
            } );
    }

    auto ThumbnailPasses::RegisterAudioFrame( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        // Draws a the typical audio preview frame

        // Create resources

        // Parse frame and upload data
        graph.RegisterPass(
            "AudioFrame_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );

        graph.RegisterPass(
            "AudioFrame_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto ThumbnailPasses::RegisterEnvironment( FrameGraph& graph ) -> void {
        mHdr = graph.Create( FGTextureDescription{}
            .SetName( "Preview.HDR" )
            .SetWidth( mWidth )
            .SetHeight( mHeight )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
        auto fullscreen{ FGPipelineDescription{}
            .SetName( "Preview.Environment" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex )
            .PushShader( "ThumbnailEnvironment_Frag.slang", FGStageType::ePixel ) };
        mEnvironmentPipeline = graph.Create( fullscreen );
        graph.RegisterPass(
            "Preview.Environment",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mSceneBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mPrefilter, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mHdr, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                context.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( mWidth ), static_cast<core::i32>( mHeight ) } )
                    .AddRenderTarget( mHdr, renderer::rhi::kColorBlack ) );
                SetViewport( context, mWidth, mHeight );
                context.PushConstants( ParametersFor( context, 2 ) );
                context.BindPipeline( mEnvironmentPipeline );
                context.Draw( 3 );
                context.EndRender();
            } );
    }

    auto ThumbnailPasses::RegisterMaterials( FrameGraph& graph ) -> void {
        mDepth = graph.Create( FGTextureDescription{}
            .SetName( "Preview.Depth" )
            .SetWidth( mWidth )
            .SetHeight( mHeight )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetFormat( renderer::rhi::Format::eD32 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget ) );
        mBloomSeed = graph.Create( FGTextureDescription{}
            .SetName( "Preview.Emission" )
            .SetWidth( mWidth )
            .SetHeight( mHeight )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
        auto meshPipeline{ FGPipelineDescription{}
            .SetName( "Preview.Opaque" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetBlendEnable( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "ThumbnailMesh_Vert.slang", FGStageType::eVertex )
            .PushShader( "ThumbnailMaterial_Frag.slang", FGStageType::ePixel ) };
        mOpaquePipeline = graph.Create( meshPipeline );
        mTransparentPipeline = graph.Create( meshPipeline
            .SetName( "Preview.Transparent" )
            .SetDepthWrite( false )
            .SetBlendEnable( true ) );
        graph.RegisterPass(
            "Preview.Materials",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                DeclareGeometryReads( builder );
                builder.UseResource( mPrefilter, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mShadow, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mDepth, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( mHdr, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( mBloomSeed, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                context.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( mWidth ), static_cast<core::i32>( mHeight ) } )
                    .AddRenderTarget( mHdr, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( mBloomSeed, renderer::rhi::kColorBlack )
                    .AddDepthTarget( mDepth ) );
                SetViewport( context, mWidth, mHeight );
                DrawMeshes( context, false );
                context.EndRender();
            } );
    }

    auto ThumbnailPasses::RegisterBloom( FrameGraph& graph ) -> void {
        const auto bloomTexture = [&]( const eastl::string& name, core::u32 width, core::u32 height ) {
            return graph.Create( FGTextureDescription{}.SetName( name ).SetWidth( width ).SetHeight( height ).SetDimensions( renderer::rhi::TextureDimension::eTexture2D ).SetFormat( renderer::rhi::Format::eRGBA16_FLOAT ).SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
        };

        for ( core::u32 level{}; level < mBloomDown.size(); ++level ) {
            const auto width{ eastl::max( 1u, mWidth >> ( level + 1 ) ) };
            const auto height{ eastl::max( 1u, mHeight >> ( level + 1 ) ) };
            mBloomDown[level] = bloomTexture( string::Format( "Preview.BloomDown{}", level ), width, height );

            if ( level < mBloomUp.size() ) {
                mBloomUp[level] = bloomTexture( string::Format( "Preview.BloomUp{}", level ), width, height );
            }
        }

        auto fullscreen{ FGPipelineDescription{}
            .SetName( "Preview.Environment" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex )
            .PushShader( "ThumbnailEnvironment_Frag.slang", FGStageType::ePixel ) };
        mBloomPipeline = graph.Create( fullscreen
            .SetName( "Preview.Bloom" )
            .PushShader( "ThumbnailBloom_Frag.slang", FGStageType::ePixel ) );
        const auto registerBloom = [&]( const eastl::string& name, FGTextureHandle source,
                                       FGTextureHandle detail, FGTextureHandle output, core::u32 level, bool upsample ) {
            const auto width{ eastl::max( 1u, mWidth >> ( level + 1 ) ) };
            const auto height{ eastl::max( 1u, mHeight >> ( level + 1 ) ) };
            graph.RegisterPass(
                name,
                FGPassType::eGraphics,
                [this, source, detail, output, upsample]( FGNodeBuilder& builder, core::Blackboard& ) {
                    builder.UseResource( mSceneBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( source, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                    if ( upsample ) {
                        builder.UseResource( detail, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }

                    builder.UseResource( output, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },
                [this, source, detail, output, level, width, height, upsample]( CommandContext& context, core::Blackboard& ) {
                    context.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( width ), static_cast<core::i32>( height ) } )
                        .AddRenderTarget( output, renderer::rhi::kColorBlack ) );
                    SetViewport( context, width, height );
                    auto parameters{ ParametersFor( context, upsample ? 5u : 4u ) };
                    parameters.mColor = context.PushTexture_SRV( source );
                    parameters.mDrawIndex = level;

                    if ( upsample ) {
                        parameters.mBloom = context.PushTexture_SRV( detail );
                    }

                    context.PushConstants( parameters );
                    context.BindPipeline( mBloomPipeline );
                    context.Draw( 3 );
                    context.EndRender();
                } );
        };

        for ( core::u32 level{}; level < mBloomDown.size(); ++level ) {
            registerBloom( string::Format( "Preview.BloomDown{}", level ),
                level == 0 ? mBloomSeed : mBloomDown[level - 1], {}, mBloomDown[level], level, false );
        }

        for ( core::i32 level{ static_cast<core::i32>( mBloomUp.size() ) - 1 }; level >= 0; --level ) {
            registerBloom( string::Format( "Preview.BloomUp{}", level ),
                level == mBloomUp.size() - 1 ? mBloomDown[level + 1] : mBloomUp[level + 1],
                mBloomDown[level], mBloomUp[level], static_cast<core::u32>( level ), true );
        }
    }

    auto ThumbnailPasses::RegisterTonemap( FrameGraph& graph ) -> void {
        mOutput = graph.Create( FGTextureDescription{}
            .SetName( "Preview.Output" )
            .SetWidth( mWidth )
            .SetHeight( mHeight )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopySource ) );
        auto fullscreen{ FGPipelineDescription{}
            .SetName( "Preview.Environment" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex )
            .PushShader( "ThumbnailEnvironment_Frag.slang", FGStageType::ePixel ) };
        fullscreen.mColorFormats.clear();
        mTonemapPipeline = graph.Create( fullscreen
            .SetName( "Preview.Tonemap" )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .PushShader( "ThumbnailTonemap_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "Preview.Tonemap",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mSceneBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mHdr, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mBloomUp[0], FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mOutput, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                context.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( mWidth ), static_cast<core::i32>( mHeight ) } )
                    .AddRenderTarget( mOutput, renderer::rhi::kColorBlack ) );
                SetViewport( context, mWidth, mHeight );
                context.PushConstants( ParametersFor( context, 3 ) );
                context.BindPipeline( mTonemapPipeline );
                context.Draw( 3 );
                context.EndRender();
            } );
    }

    auto ThumbnailPasses::RegisterReadback( FrameGraph& graph ) -> void {
        mReadbackBuffer = graph.Create( FGBufferDescription{}
            .SetName( "Preview.Readback" )
            .SetSizeBytes( core::usize{ mWidth } * mHeight * 4 )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eReadback ) );
        graph.RegisterPass(
            "Preview.Readback",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mOutput, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( mReadbackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                if ( mReadback ) {
                    context.Copy( mReadbackBuffer, mOutput );
                }
            } );
    }

    auto ThumbnailPasses::RegisterSampledOutput( FrameGraph& graph ) -> void {
        // The ImGui consumer is outside this graph. Leave its image in sampled
        // layout after readback; the next execution starts with the graph barrier.
        graph.RegisterPass(
            "Preview.SampledOutput",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                // Observe the readback buffer's producer so the sampled-output
                // transition follows the copy, through resource dependencies.
                builder.UseResource( mReadbackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( mOutput, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            []( CommandContext&, core::Blackboard& ) {
            } );
    }

    auto ThumbnailPasses::DeclareGeometryReads( FGNodeBuilder& builder ) const -> void {
        builder.UseResource( mPaletteBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
        builder.UseResource( mSceneBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
        builder.UseResource( mSceneBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
        builder.UseResource( mDrawBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
        builder.UseResource( mDrawBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

        for ( const auto& mesh: mMeshes ) {
            builder.UseResource( mesh.mVertices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( mesh.mIndices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
        }
    }

    auto ThumbnailPasses::Prepare( const ThumbnailPreviewSettings& settings, material::MaterialHandle material, bool readback ) -> void {
        // Reject an unsupported override instead of silently showing the mesh's
        // surface material. Text is routed to a glyph preview by ThumbnailRenderer.
        if ( material && !dynamic_cast<const material::SurfaceMaterial*>( material.GetPtr() ) &&
             !dynamic_cast<const material::SkyboxMaterial*>( material.GetPtr() ) &&
             !dynamic_cast<const material::PostProcessMaterial*>( material.GetPtr() ) ) {
            MKT_THROW_RUNTIME_ERROR( "This material does not support a mesh preview." );
        }

        mSettings = settings;
        mSceneData.mPreviewOptions = { settings.mInfiniteGrid ? 1.0f : 0.0f, 0, 0, 0 };
        mOverride = eastl::move( material );
        mReadback = readback;

        const core::f32 yaw{ glm::radians( settings.mYaw ) };
        const core::f32 pitch{ glm::radians( glm::clamp( settings.mPitch, -80.0f, 80.0f ) ) };
        const core::float3 backward{ glm::sin( yaw ) * glm::cos( pitch ), glm::sin( pitch ), glm::cos( yaw ) * glm::cos( pitch ) };
        const core::f32 aspect{ static_cast<core::f32>( mWidth ) / mHeight };
        const core::f32 fieldOfView{ glm::radians( 42.0f ) };
        core::float3 target{ 0.0f, 0.9f, 0.0f };
        core::f32 distance{ glm::clamp( settings.mDistance, 2.0f, 20.0f ) };

        if ( settings.mFitMaterialSphere ) {
            // A sphere's silhouette is tangent to the view cone: sin(theta)=r/d.
            // Unlike an AABB fit, this leaves no padding at the square's sides.
            target.y = mNormalizedHalfExtent.y;
            const auto halfAngle{ glm::min( fieldOfView * 0.5f, glm::atan( glm::tan( fieldOfView * 0.5f ) * aspect ) ) };
            const auto radius{ glm::max( mNormalizedHalfExtent.x, glm::max( mNormalizedHalfExtent.y, mNormalizedHalfExtent.z ) ) };
            distance = radius / glm::sin( halfAngle ) * 1.002f;
        }
        else if ( settings.mFrameModelBounds ) {
            // Normalization places the model's bottom at zero. Aim at its actual
            // center so short/wide assets do not sit at the bottom of the tile.
            target.y = mNormalizedHalfExtent.y;
            const core::float3 right{ glm::normalize( glm::cross( core::float3{ 0, 1, 0 }, backward ) ) };
            const core::float3 up{ glm::cross( backward, right ) };
            const core::f32 verticalLimit{ glm::tan( fieldOfView * 0.5f ) * 0.98f };
            const core::f32 horizontalLimit{ verticalLimit * aspect };
            distance = 0.1f;
            // Fit every bounds corner in perspective, accounting for its depth.
            // A small border keeps roof tips and wide bases away from tile edges.
            for ( core::u32 corner{}; corner < 8; ++corner ) {
                const core::float3 offset{ mNormalizedHalfExtent * core::float3{
                                                                       corner & 1u ? 1.0f : -1.0f,
                                                                       corner & 2u ? 1.0f : -1.0f,
                                                                       corner & 4u ? 1.0f : -1.0f } };
                const core::f32 depth{ glm::dot( offset, backward ) };
                distance = glm::max( distance, depth + glm::max( 0.06f,
                                                           glm::max( glm::abs( glm::dot( offset, right ) ) / horizontalLimit,
                                                               glm::abs( glm::dot( offset, up ) ) / verticalLimit ) ) );
            }
        }

        const core::float3 eye{ target + distance * backward };
        auto projection{ glm::perspectiveRH_ZO( fieldOfView, aspect, 0.03f, 50.0f ) };

        // The RHI already maps upward clip-space Y to top-left render targets
        // (a negative-height viewport on Vulkan). Flipping the projection again
        // reverses front faces, making single-sided previews show their interiors.
        mSceneData.mViewProjection = projection * glm::lookAtRH( eye, target, core::float3{ 0, 1, 0 } );
        mSceneData.mInverseViewProjection = glm::inverse( mSceneData.mViewProjection );
        const core::f32 azimuth{ glm::radians( settings.mLightYaw ) };
        const core::f32 elevation{ glm::radians( glm::clamp( settings.mLightElevation, 5.0f, 85.0f ) ) };
        const core::float3 light{ glm::cos( elevation ) * glm::sin( azimuth ), glm::sin( elevation ), glm::cos( elevation ) * glm::cos( azimuth ) };
        auto shadowProjection{ glm::orthoRH_ZO( -5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 20.0f ) };
        // Use the same clip-space convention for the camera and shadow camera.
        mSceneData.mLightViewProjection = shadowProjection * glm::lookAtRH( target + light * 8.0f, target, core::float3{ 0, 1, 0 } );
        mSceneData.mCamera = core::float4{ eye, settings.mExposure };
        mSceneData.mLightDirection = core::float4{ light, settings.mShadows ? 1.0f : 0.0f };
        mSceneData.mLightColor = core::float4{ 1.0f, 0.96f, 0.90f, settings.mLightIntensity };
        mSceneData.mSettings = { static_cast<core::f32>( mWidth ), static_cast<core::f32>( mHeight ), settings.mEnvironmentIntensity, settings.mBackgroundBlur };
        auto bloom{ settings.mBloom };
        bloom.Sanitize();
        mSceneData.mBloom = { bloom.mThreshold, bloom.mEnabled ? bloom.mIntensity : 0.0f, bloom.mScatter, bloom.mRadius };
    }

    auto ThumbnailPasses::Upload( CommandContext& context ) -> void {
        for ( core::usize index{}; index < mMeshes.size(); ++index ) {
            auto& mesh{ mMeshes[index] };
            auto* physical{ dynamic_cast<material::SurfaceMaterial*>( mOverride.GetPtr() ) };
            auto& material{ physical ? *physical : *mesh.mMaterial };
            material.UpdateTextureResidency();
            auto& data{ mDrawData[index] };
            data = {};
            data.mModel = mNormalization * mesh.mMesh->GetTransform();

            if ( !mesh.mJointCount ) {
                data.mModel *= mesh.mMesh->GetSkinBinding().mBindPoseTransform;
            }

            data.mNormal = glm::transpose( glm::inverse( data.mModel ) );
            data.mJointCount = mesh.mJointCount;
            data.mPalette = context.GetDeviceBufferAddress( mPaletteBuffer ) + mesh.mPaletteOffset * sizeof( core::float4x4 );
            data.mBaseColor = material.GetBaseColorFactor();
            data.mEmission = core::float4{ material.GetEmissiveFactor(), material.GetEmissiveStrength() };
            data.mFactors = { material.GetMetallicFactor(), material.GetRoughnessFactor(), material.GetNormalScale(), material.GetOcclusionStrength() * material.GetAoFactor() };
            if ( material.GetShadingModel() == material::ShadingModel::eConductor ) {
                data.mBaseColor *= core::float4{ material.GetConductorReflectance(), 1 };
                data.mFactors.x = 1;
            }
            data.mFlags = { static_cast<core::f32>( material.GetAlphaMask() ), material.GetAlphaMaskCutoff(), material.IsDoubleSided() ? 1.0f : 0.0f, 0.0f };
            const auto textureID{ [&]( material::MapType type ) -> core::i32 {
                if ( !material.IsTextureUsed( type ) ) {
                    return -1;
                }

                const auto texture{ material.GetTexture( type ) };
                return texture ? static_cast<core::i32>( context.PushTexture_SRV( context.ImportTexture( texture ) ) ) : -1;
            } };
            data.mClearCoatNormalIndex = textureID( material::MapType::eClearCoatNormal );
            data.mClearCoatNormalTextureSet = material.GetClearCoatNormalTextureSet();
            data.mClearCoatNormalScale = material.GetClearCoatNormalScale();
            data.mUvTransform = material.GetUvTransform();
            data.mTextures0 = { textureID( material::MapType::eBaseColor ), textureID( material::MapType::eNormal ), textureID( material::MapType::eMetallicRoughness ), textureID( material::MapType::eEmissive ) };
            data.mTextures1 = { textureID( material::MapType::eAmbientOcclusion ), textureID( material::MapType::eMetallic ), textureID( material::MapType::eRoughness ), -1 };
            data.mUv0 = { material.GetBaseColorTextureSet(), material.GetNormalTextureSet(), material.GetMetallicRoughnessTextureSet(), material.GetEmissiveTextureSet() };
            // A reflected instance reverses winding even though the source mesh
            // keeps its index order. Correct front-facing tests in the shaders.
            data.mUv1 = { material.GetOcclusionTextureSet(), glm::determinant( core::float3x3{ data.mModel } ) < 0.0f ? 1 : 0, 0, 0 };
            data.mUv1[3] = material.IsBloomy() ? 1 : 0;
            const auto& shading{ material.GetSurfaceShading() };
            data.mSurfaceShading = { shading.mCoatWeight, shading.mCoatRoughness, shading.mToonThreshold, shading.mToonSoftness };

            if ( material.GetShadingModel() == material::ShadingModel::eDielectric ) {
                const auto& dielectric{ material.GetDielectric() };
                data.mSurfaceShading = { dielectric.mIndexOfRefraction, dielectric.mTransmission, dielectric.mThickness, dielectric.mAttenuationDistance };
                data.mDielectricAttenuation = { dielectric.mAttenuationColor, dielectric.mAttenuationDistance };
            }

            data.mShadingModel = { core::as<core::f32>( material.GetShadingModel() ), core::as<core::f32>( shading.mCellBands ), 0, 0 };

            if ( mOverride && !physical ) {
                // Environment and grading previews have explicit shader modes;
                // neither should inherit textures from the sample mesh.
                data.mTextures0.fill( -1 );
                data.mTextures1.fill( -1 );
                data.mUv0.fill( 0 );
                data.mUv1[3] = 0;
                data.mBaseColor = core::float4{ 0.5f, 0.5f, 0.5f, 1.0f };
                data.mEmission = core::float4{ 0.0f };
                data.mFactors = { 0.0f, 0.5f, 1.0f, 1.0f };
                data.mFlags = core::float4{ 0.0f };

                if ( const auto* sky{ dynamic_cast<const material::SkyboxMaterial*>( mOverride.GetPtr() ) } ) {
                    const bool latLong{ sky->GetType() == material::SkyboxType::eEquirectangular };
                    data.mUv1[2] = latLong ? 2 : 3;
                    data.mFactors.x = sky->GetAmbientScale();
                    constexpr material::SkyboxFace faces[]{ material::SkyboxFace::eRight, material::SkyboxFace::eLeft,
                        material::SkyboxFace::eTop, material::SkyboxFace::eBottom, material::SkyboxFace::eBack, material::SkyboxFace::eFront };

                    for ( core::u32 face{}; face < ( latLong ? 1u : 6u ); ++face ) {
                        const auto texture{ latLong ? sky->GetEquirectangular() : sky->GetFace( faces[face] ) };
                        const core::i32 id{ texture ? static_cast<core::i32>( context.PushTexture_SRV( context.ImportTexture( texture ) ) ) : -1 };
                        ( face < 4 ? data.mTextures0[face] : data.mTextures1[face - 4] ) = id;

                        if ( texture && ( texture->GetFormat() == renderer::rhi::Format::eRGBA8_UNORM || texture->GetFormat() == renderer::rhi::Format::eBGRA8_UNORM ) ) {
                            data.mPadding |= 1u << face;
                        }
                    }
                }
                else if ( const auto* post{ dynamic_cast<const material::PostProcessMaterial*>( mOverride.GetPtr() ) } ) {
                    data.mUv1[2] = 4;
                    data.mEmission = post->GetTintColor();
                    data.mFactors.z = post->GetContrast();
                    data.mFactors.w = post->GetSaturation();
                }
            }

            data.mVertices = context.GetDeviceBufferAddress( mesh.mVertices );
            data.mIndices = context.GetDeviceBufferAddress( mesh.mIndices );
        }

        auto& ground{ mDrawData.back() };
        ground = {};
        ground.mBaseColor = { 0.32f, 0.35f, 0.39f, 1.0f };
        ground.mFactors = { 0.0f, 0.8f, 1.0f, 1.0f };
        ground.mFlags.w = 1.0f;
        context.CopyBuffer( mSceneBuffer, mSceneData, 0 );
        context.CopyBuffer( mDrawBuffer, 0, mDrawData.data(), mDrawData.size() * sizeof( DrawData ) );
        context.CopyBuffer( mPaletteBuffer, 0, mPalettes.data(), mPalettes.size() * sizeof( core::float4x4 ) );
    }

    auto ThumbnailPasses::RegisterPrefilter( FrameGraph& graph ) -> void {
        mClampSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Preview.Clamp" )
            .SetMipLevels( static_cast<core::f32>( kPrefilterMipCount - 1 ) )
            .SetFilter( renderer::rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge ) );
        // Use ForwardRenderer's cubemap area-mip shader and shared GGX convolution.
        const auto source{ graph.Create( FGTextureDescription{}
            .SetName( "Preview.EnvironmentCube" )
            .SetWidth( 1024 )
            .SetHeight( 1024 )
            .SetMipCount( 11 )
            .SetDimensions( renderer::rhi::TextureDimension::eTextureCube )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::CopyDest | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) ) };
        const auto scratch{ graph.Create( FGTextureDescription{}
            .SetName( "Preview.EnvironmentScratch" )
            .SetWidth( 1024 )
            .SetHeight( 1024 )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopySource ) ) };
        auto description{ FGPipelineDescription{}
            .SetName( "Preview.EnvironmentCube" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex ) };
        auto projectionDescription{ description };
        projectionDescription.PushShader( "ThumbnailEnvironmentCube_Frag.slang", FGStageType::ePixel );
        const auto projection{ graph.Create( projectionDescription ) };
        description.SetName( "Preview.EnvironmentMips" ).PushShader( "SkyboxMip_Frag.slang", FGStageType::ePixel );
        const auto mipPipeline{ graph.Create( description ) };
        // Render each face, then copy it in a separate pass. The graph orders
        // scratch reuse and completes each cube mip before the next one samples it.
        for ( core::u32 mip{}; mip < 11; ++mip ) {
            const core::u32 size{ 1024u >> mip };

            for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                graph.RegisterPass(
                    string::Format( "Preview.EnvironmentMip{}_Face{}_Render", mip, face ),
                    FGPassType::eGraphics,
                    [this, source, scratch, mip]( FGNodeBuilder& builder, core::Blackboard& ) {
                        if ( mip == 0 ) {
                            builder.UseResource( mSceneBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        }
                        else {
                            builder.UseResource( source, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        }

                        builder.UseResource( scratch, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                    },
                    [this, source, scratch, projection, mipPipeline, mip, face, size]( CommandContext& context, core::Blackboard& ) {
                        context.BeginRender( RenderBeginDescription{}
                            .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( size ), static_cast<core::i32>( size ) } )
                            .AddRenderTarget( scratch, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear ) );
                        SetViewport( context, size, size );

                        if ( mip == 0 ) {
                            auto parameters{ ParametersFor( context, 6 ) };
                            parameters.mDrawIndex = face;
                            context.PushConstants( parameters );
                            context.BindPipeline( projection );
                        }
                        else {
                            struct MipParameters {
                                core::u32 cube, sampler, level, face, sourceSize, destinationSize;
                            };
                            const MipParameters parameters{ context.PushTexture_SRV( source ), context.PushSampler( mSampler ),
                                mip - 1, face, size * 2, size };
                            context.PushConstants( parameters );
                            context.BindPipeline( mipPipeline );
                        }

                        context.Draw( 3 );
                        context.EndRender();
                    } );

                graph.RegisterPass(
                    string::Format( "Preview.EnvironmentMip{}_Face{}_Copy", mip, face ),
                    FGPassType::eTransfer,
                    [source, scratch]( FGNodeBuilder& builder, core::Blackboard& ) {
                        builder.UseResource( scratch, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                        builder.UseResource( source, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                    },
                    [source, scratch, mip, face, size]( CommandContext& context, core::Blackboard& ) {
                        context.CopyTexture( scratch, renderer::rhi::TextureSlice{ .mWidth = size, .mHeight = size },
                            source, renderer::rhi::TextureSlice{ .mWidth = size, .mHeight = size, .mMipLevel = mip, .mArrayLayer = face } );
                    } );
            }
        }

        mPrefilter = graph.Create( FGTextureDescription{}
            .SetName( "Preview.Prefilter" )
            .SetWidth( kPrefilterSize )
            .SetHeight( kPrefilterSize )
            .SetMipCount( kPrefilterMipCount )
            .SetDimensions( renderer::rhi::TextureDimension::eTextureCube )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
        mPrefilterPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Preview.Prefilter" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex )
            .PushShader( "ThumbnailPrefilter_Frag.slang", FGStageType::ePixel ) );
        graph.RegisterPass(
            "Preview.Prefilter",
            FGPassType::eGraphics,
            [this, source]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( source, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mPrefilter, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, source]( CommandContext& context, core::Blackboard& ) {
                auto parameters{ ParametersFor( context, 6 ) };
                parameters.mPrefilter = context.PushTexture_SRV( source );
                parameters.mPrefilterMipCount = kPrefilterMipCount;

                for ( core::u32 mip{}; mip < kPrefilterMipCount; ++mip ) {
                    const core::u32 size{ kPrefilterSize >> mip };
                    parameters.mColor = mip;

                    for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        parameters.mDrawIndex = face;
                        context.BeginRender( RenderBeginDescription{}
                            .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( size ), static_cast<core::i32>( size ) } )
                            .AddRenderTarget( mPrefilter, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear, face, mip ) );

                        SetViewport( context, size, size );

                        context.PushConstants( parameters );
                        context.BindPipeline( mPrefilterPipeline );

                        context.Draw( 3 );
                        context.EndRender();
                    }
                }
            } );
    }

    auto ThumbnailPasses::ParametersFor( CommandContext& context, core::u32 mode ) const -> Parameters {
        Parameters parameters{};
        parameters.mScene = context.GetDeviceBufferAddress( mSceneBuffer );
        parameters.mDraws = context.GetDeviceBufferAddress( mDrawBuffer );
        parameters.mSampler = context.PushSampler( mSampler );
        parameters.mClampSampler = context.PushSampler( mClampSampler );
        parameters.mMode = mode;

        if ( mode == 0 || mode == 2 ) {
            parameters.mPrefilter = context.PushTexture_SRV( mPrefilter );
            parameters.mPrefilterMipCount = kPrefilterMipCount;
        }

        if ( mode == 0 ) {
            parameters.mShadow = context.PushTexture_SRV( mShadow );
        }

        if ( mode == 3 ) {
            parameters.mColor = context.PushTexture_SRV( mHdr );
            parameters.mBloom = context.PushTexture_SRV( mBloomUp[0] );
        }

        if ( mSettings.mEnvironment ) {
            parameters.mEnvironment = static_cast<core::i32>( context.PushTexture_SRV( context.ImportTexture( mSettings.mEnvironment ) ) );
        }

        return parameters;
    }

    auto ThumbnailPasses::DrawMeshes( CommandContext& context, bool shadow ) -> void {
        auto parameters{ ParametersFor( context, shadow ? 1 : 0 ) };
        // Ground and opaque/masked geometry establish depth before transparent
        // surfaces. Transparent meshes are sorted back-to-front by mesh center.
        if ( !shadow && mSettings.mGround ) {
            parameters.mDrawIndex = static_cast<core::u32>( mMeshes.size() );
            context.PushConstants( parameters );
            context.BindPipeline( mOpaquePipeline );
            context.Draw( 6 );
        }

        eastl::vector<core::u32> transparent{};

        for ( core::u32 index{}; index < mMeshes.size(); ++index ) {
            if ( mDrawData[index].mFlags.x == static_cast<core::f32>( material::AlphaMode::eBlend ) ) {
                if ( !shadow ) {
                    transparent.push_back( index );
                }

                continue;
            }

            parameters.mDrawIndex = index;
            context.PushConstants( parameters );
            context.BindPipeline( shadow ? mShadowPipeline : mOpaquePipeline );
            context.Draw( static_cast<core::u32>( mMeshes[index].mMesh->GetCollisionIndices().size() ) );
        }

        const core::float3 eye{ mSceneData.mCamera };
        eastl::sort( transparent.begin(), transparent.end(), [&]( core::u32 left, core::u32 right ) {
            const core::float3 a{ mNormalization * core::float4{ mMeshes[left].mCenter, 1.0f } };
            const core::float3 b{ mNormalization * core::float4{ mMeshes[right].mCenter, 1.0f } };
            return glm::dot( a - eye, a - eye ) > glm::dot( b - eye, b - eye );
        } );

        for ( const core::u32 index: transparent ) {
            parameters.mDrawIndex = index;
            context.PushConstants( parameters );
            context.BindPipeline( mTransparentPipeline );
            context.Draw( static_cast<core::u32>( mMeshes[index].mMesh->GetCollisionIndices().size() ) );
        }
    }

    auto ThumbnailPasses::SetViewport( CommandContext& context, core::u32 width, core::u32 height ) const -> void {
        context.SetViewportState( renderer::rhi::ViewportState{}
            .AddViewportAndScissorRect( renderer::rhi::Viewport{ as<core::f32>( width ), as<core::f32>( height ) } ) );
    }

    auto ThumbnailPasses::GetOutput() const -> FGTextureHandle {
        return mOutput;
    }

    auto ThumbnailPasses::ReadImage( const FGResourceManager& resources ) const -> asset::ImageHandle {
        const void* bytes{ resources.GetBufferMappedAddress( mReadbackBuffer ) };

        if ( !bytes ) {
            MKT_THROW_RUNTIME_ERROR( "GPU preview readback is not mapped." );
        }

        auto image{ asset::ImageHandle::New() };
        image->mWidth = mWidth;
        image->mHeight = mHeight;
        image->mChannels = 4;
        image->mFormat = asset::ImageFormat::eRGBA8_UINT;
        image->mBufferSpan = memory::BufferSpanHandle::New( bytes, core::usize{ mWidth } * mHeight * 4 );
        return image;
    }
}// namespace mikoto::renderer
