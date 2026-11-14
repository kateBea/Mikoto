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

#include <array>
#include <cmath>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/optional.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/AssetRegistryService.hh>

#include <Animation/Animator.hh>
#include <Animation/Skeleton.hh>
#include <Animation/AnimationSystem.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/DebugModule.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::renderer::rhi;

    // ozz sample: InitPostureRendering's bone mesh and BoneShader's parent/child frame.
    // https://github.com/guillaumeblanc/ozz-animation/blob/master/samples/framework/internal/renderer_impl.cc#L267
    // https://github.com/guillaumeblanc/ozz-animation/blob/master/samples/framework/internal/shader.cc#L518
    static auto BuildBoneMesh() -> std::array<float4, 24> {
        const std::array<float4, 6> positions{
            float4{ 1, 0, 0, 1 }, float4{ .2f, .1f, .1f, 1 },
            float4{ .2f, .1f, -.1f, 1 }, float4{ .2f, -.1f, -.1f, 1 },
            float4{ .2f, -.1f, .1f, 1 }, float4{ 0, 0, 0, 1 }
        };
        constexpr std::array<u32, 24> indices{ 0, 2, 1, 5, 1, 2, 0, 3, 2, 5, 2, 3,
            0, 4, 3, 5, 3, 4, 0, 1, 4, 5, 4, 1 };
        std::array<float4, 24> vertices{};

        for ( usize index{}; index < indices.size(); ++index ) {
            vertices[index] = positions[indices[index]];
        }

        return vertices;
    }

    static auto BuildArmatureBone( const float4x4& parent, const float4x4& child,
        const float4x4& world, const float4& color, ArmatureBone& bone ) -> bool {
        const float3 direction{ child[3] - parent[3] };
        const f32 length{ glm::length( direction ) };

        if ( length <= 1e-7f ) {
            return false;
        }

        // Use the less parallel parent axis, as ozz does, to avoid a collapsed bone frame.
        const float3 binormal{ glm::abs( glm::dot( float3{ parent[2] }, direction ) ) <
                glm::abs( glm::dot( float3{ parent[0] }, direction ) ) ? parent[2] : parent[0] };
        const auto side{ glm::cross( binormal, direction ) };

        if ( glm::dot( side, side ) <= 1e-14f ) {
            return false;
        }

        const auto y{ length * glm::normalize( side ) };
        const auto z{ length * glm::normalize( glm::cross( direction, y ) ) };
        bone = { world * float4{ direction, 0 }, world * float4{ y, 0 },
            world * float4{ z, 0 }, world * float4{ float3{ parent[3] }, 1 }, color };
        return true;
    }

    DebugModule::DebugModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto DebugModule::SetMultisampling( rhi::Multisampling multisampling ) -> void {
    }

    auto DebugModule::SetScene( const scene::Scene* scene ) -> void {
        mScene = scene;
    }

    auto DebugModule::SetArmatureMode( ArmatureDebugMode mode ) -> void {
        mArmatureMode = mode;
    }

    auto DebugModule::GatherArmatures() -> void {
        mArmatureBones.clear();
        auto* animations{ animation::AnimationSystem::GetPtr() };

        if ( !mScene || !animations || mArmatureMode == ArmatureDebugMode::eDisabled ) {
            return;
        }

        // Draw each animator's rigs once. Submeshes already share these model-space poses.
        for ( const auto [entity, tag, transform, component]:
            mScene->GetRegistry().view<scene::TagComponent, scene::TransformComponent, scene::AnimatorComponent>().each() ) {
            const auto* animator{ animations->GetAnimator( component.GetAnimatorID() ) };

            if ( !tag.IsActive() || !animator || !animator->GetModel() ) {
                continue;
            }

            const auto model{ animator->GetModel() };
            const float4 color{ 0.78f, 0.86f, 0.94f, 1 };

            for ( u32 rig{}; rig < model->GetRigs().size(); ++rig ) {
                const auto* skeleton{ model->GetSkeleton( rig ) };
                const auto* ozz{ skeleton ? skeleton->GetOzzSkeleton() : nullptr };

                if ( !ozz ) {
                    continue;
                }

                const auto parents{ ozz->joint_parents() };
                const auto& pose{ animator->GetJointModelMatrices( rig ) };

                for ( usize joint{}; joint < parents.size() && joint < pose.size(); ++joint ) {
                    const auto parent{ parents[joint] };

                    if ( parent < 0 || static_cast<usize>( parent ) >= pose.size() ) {
                        continue;
                    }

                    ArmatureBone bone{};

                    if ( BuildArmatureBone( pose[parent], pose[joint], transform.GetWorldTransform(), color, bone ) ) {
                        if ( mArmatureBones.size() == kMaxArmatureBones ) {
                            return;
                        }

                        mArmatureBones.push_back( bone );
                    }
                }
            }
        }
    }

    auto DebugModule::RegisterArmaturePass( FrameGraph& graph ) -> void {
        const auto [width, height]{ InferDimensions( mResolution ) };
        const auto vertices{ graph.Create( FGBufferDescription{}
            .SetName( "Armature_BoneMesh" )
            .SetSizeBytes( 24 * sizeof( float4 ) )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest ) ) };

        const auto bones{ graph.Create( FGBufferDescription{}
            .SetName( "Armature_Instances" )
            .SetSizeBytes( kMaxArmatureBones * sizeof( ArmatureBone ) )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest ) ) };

        // A private depth buffer keeps bones in front of the model while preserving their own depth order.
        const auto depth{ graph.Create( FGTextureDescription{}
            .SetName( "Armature_Depth" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eD32 )
            .SetUsage( TextureUsageFlagsBits::DepthTarget ) ) };

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Armature_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetCullMode( CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetBlendEnable( false )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "Armature_Vert.slang", FGStageType::eVertex )
            .PushShader( "Armature_Frag.slang", FGStageType::ePixel ) ) };

        graph.RegisterPass(
            "Armature_Upload",
            FGPassType::eTransfer,
            [vertices, bones]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( vertices, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( bones, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, vertices, bones]( CommandContext& ctx, core::Blackboard& ) {
                GatherArmatures();

                if ( !mArmatureBones.empty() ) {
                    const auto mesh{ BuildBoneMesh() };
                    ctx.CopyBuffer( vertices, 0, mesh.data(), sizeof( mesh ) );
                    ctx.CopyBuffer( bones, 0, mArmatureBones.data(), mArmatureBones.size() * sizeof( ArmatureBone ) );
                }
            } );

        graph.RegisterPass(
            "Armature_Render",
            FGPassType::eGraphics,
            [vertices, bones, depth]( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( vertices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( bones, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( depth, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( b.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, vertices, bones, depth, pipeline]( CommandContext& ctx, core::Blackboard& b ) {
                if ( mArmatureBones.empty() ) {
                    return;
                }

                struct Parameters {
                    SPointer mVertices{};
                    SPointer mBones{};
                    SPointer mCamera{};
                } params{ ctx.GetDeviceBufferAddress( vertices ), ctx.GetDeviceBufferAddress( bones ),
                    ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ) };

                const auto [width, height]{ InferDimensions( mResolution ) };

                ctx.PushConstants( params );
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( Rect{ as<i32>( width ), as<i32>( height ) } )
                    .AddRenderTarget( b.Get<PresentationPassData>().mColorImage, Color{}, LoadOp::eLoad )
                    .AddDepthTarget( depth, LoadOp::eClear ) );
                ctx.SetViewportState( ViewportState{}.AddViewportAndScissorRect( Viewport{ as<f32>( width ), as<f32>( height ) } ) );
                ctx.BindPipeline( pipeline );
                ctx.Draw( 24, as<u32>( mArmatureBones.size() ) );
                ctx.EndRender();
            } );
    }

    auto DebugModule::RegisterTrianglePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        TrianglePassData &passData{ graph.GetOrCreate<TrianglePassData>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "TrianglePass_ColorImage01" )
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eBGRA8_UNORM ) };
        passData.mColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "TrianglePass_DepthImage01" )
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };
        passData.mDepthTarget = graph.Create( depthImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "TrianglePass_Pipeline01" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( "HelloTriangleFG_Vert.slang", FGStageType::eVertex )
            .PushShader( "HelloTriangleFG_Frag.slang", FGStageType::ePixel ) };
        passData.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<TrianglePassData>(
            "SimpleTriangle",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, TrianglePassData &data ) {
                b.UseResource( data.mColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
            },
            []( CommandContext &ctx, core::Blackboard &b ) -> void {
                const auto &data{ b.Get<TrianglePassData>() };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ 1920, 1080 } )
                    .AddDepthTarget( data.mDepthTarget )
                    .AddRenderTarget( data.mColorTarget, renderer::rhi::kColorGreen, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( 1920, 1080 ) ) );

                ctx.BindPipeline( data.mPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto DebugModule::RegisterTexturePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        auto &info{ graph.GetOrCreate<TexturePassData>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "TexturePass_ColorImage01" )
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                renderer::rhi::TextureUsageFlagsBits::ShaderResource | 
                renderer::rhi::TextureUsageFlagsBits::CopySource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "TexturePass_DepthImage01" )
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };
        info.mDepthTarget = graph.Create( depthImage );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "TexturePass_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat )
            .SetBorderColor( renderer::rhi::kColorWhite ) };

        info.mSampler = graph.Create( samplerDes );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "TexturePass_Pipeline01" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .PushShader( "HelloTexture_Vert.slang", FGStageType::eVertex )
            .PushShader( "HelloTexture_Frag.slang", FGStageType::ePixel ) };

        info.mPipeline = graph.Create( pipelineBuilder );
        info.mImportedTexture = graph.ImportTexture( "Resources/Textures/diffuse.jpg" );

        const auto &formatInfo{ rhi::GetFormatInfo( renderer::rhi::Format::eRGBA8_UNORM ) };
        auto bufferDesc{ FGBufferDescription{}
            .SetName( "TexturePass_Pipeline01_ReadbackBuffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | 
                renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 1920 * 1080 * formatInfo.mBytesPerBlock )
            .SetHeapType( renderer::rhi::HeapType::eReadback ) };
        info.mReadBackBuffer = graph.Create( bufferDesc );

        graph.RegisterPass<TexturePassData>(
            "SimpleTexture",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, TexturePassData &data ) {
                b.UseResource( data.mColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mImportedTexture, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            },
            []( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
                const auto &data{ blackboard.Get<TexturePassData>() };
                struct DrawParams {
                    core::u32 mTextureIndex{};
                    core::u32 mSamplerIndex{};
                } params{
                    .mTextureIndex = ctx.PushTexture_SRV( data.mImportedTexture ),
                    .mSamplerIndex = ctx.PushSampler( data.mSampler ),
                };

                ctx.PushConstants( params );

                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ 1920, 1080 } )
                    .AddDepthTarget( data.mDepthTarget )
                    .AddRenderTarget( data.mColorTarget, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( 1920, 1080 ) ) );

                ctx.BindPipeline( data.mPipeline );
                ctx.Draw( 4 );

                ctx.EndRender();
            } );

        // Maybe change this to compute
        graph.RegisterPass(
            "SimpleTexture_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, core::Blackboard &blackboard ) {
                const auto &texturePassData{ blackboard.Get<TexturePassData>() };

                builder.UseResource( texturePassData.mReadBackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( texturePassData.mColorTarget, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            },
            []( CommandContext &ctx, core::Blackboard &b ) {
                const auto &texturePassData{ b.Get<TexturePassData>() };

                // Try doing this with a compute shader instead
                // This takes a lot, in Nsight Graphics this shows 0.32ms
                ctx.Copy( texturePassData.mReadBackBuffer, texturePassData.mColorTarget );
            } );

        graph.RegisterReadback(
            []( core::Blackboard &blackboard, const FGResourceManager &manager ) {
                const auto &formatInfo{ rhi::GetFormatInfo( renderer::rhi::Format::eRGBA8_UNORM ) };
                const auto &data{ blackboard.Get<TexturePassData>() };

                core::usize totalBytes{ 1920u * 1080u * formatInfo.mBytesPerBlock };

                eastl::vector<core::u32> blob{};
                blob.resize( totalBytes / sizeof( core::u32 ) );

                if ( const void *mappedAddress{ manager.GetBufferMappedAddress( data.mReadBackBuffer ) } ) {
                    std::memcpy( blob.data(), mappedAddress, totalBytes );
                }

                asset::WriteImage( "Resources/image.png", blob.data(), 1920, 1080, asset::ImageFormat::eRGBA8_UINT );
            }, false );
    }

    auto DebugModule::RegisterSimpleComputePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct SimpleCompute {
            FGBufferHandle mComputeBuffer{};
            FGBufferHandle mReadbackBuffer{};

            FGPipelineHandle mPipeline{};

            core::u32 mLocalSize{ 64 };
            core::u32 mNumbersCount{ 30 };
            core::u32 mGroupCount{ ( mNumbersCount + mLocalSize - 1 ) / mLocalSize };
        };

        struct MyStruct {
            core::i32 x{};
            core::i32 y{};
            core::i32 prime{};
            core::i32 _pad{};
        };

        // Create the resources
        auto &simpleCompute{ graph.GetOrCreate<SimpleCompute>() };

        // GPU buffer (written by compute shader)
        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "SimpleCompute_ComputeBuffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopySource )
            .SetElementsSize( simpleCompute.mNumbersCount, MKT_SIZEOF( MyStruct ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        simpleCompute.mComputeBuffer = graph.Create( gpuBufferDesc );

        // Readback buffer (CPU visible, copy destination)
        auto readbackDesc{ FGBufferDescription{}
            .SetName( "SimpleCompute_ReadBackBuffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( simpleCompute.mNumbersCount, MKT_SIZEOF( MyStruct ) )
            .SetHeapType( renderer::rhi::HeapType::eReadback ) };
        simpleCompute.mReadbackBuffer = graph.Create( readbackDesc );

        // Pipeline
        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SimpleCompute_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "BasicCompute_Comp.slang", FGStageType::eCompute ) };
        simpleCompute.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute",
            FGPassType::eCompute,
            []( FGNodeBuilder &b, SimpleCompute &data ) {
                b.UseResource( data.mComputeBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            []( CommandContext &ctx, core::Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };
                struct ComputeParams {
                    SPointer mBufferIndex{};
                    SArraySize mBufferSize{};
                } params{
                    .mBufferIndex = ctx.GetDeviceBufferAddress( data.mComputeBuffer ),
                    .mBufferSize = data.mNumbersCount,
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( data.mPipeline );

                ctx.Dispatch( data.mGroupCount, 1, 1 );
            } );

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, SimpleCompute &data ) {
                b.UseResource( data.mReadbackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mComputeBuffer, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            },
            []( CommandContext &ctx, core::Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };
                ctx.CopyBuffer( data.mReadbackBuffer, data.mComputeBuffer );
            } );

        graph.SetExecutionPolicy( "SimpleCompute_Readback", FGExecutionPolicy::eOnce );

        graph.RegisterReadback(
            []( core::Blackboard &blackboard, const FGResourceManager &manager ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };

                eastl::vector<MyStruct> myStructs( data.mNumbersCount );
                const core::usize sizeBytes{ data.mNumbersCount * MKT_SIZEOF( MyStruct ) };

                if ( const void *mappedAddress{ manager.GetBufferMappedAddress( data.mReadbackBuffer ) } ) {
                    std::memcpy( myStructs.data(), mappedAddress, sizeBytes );
                }
            } );
    }
}// namespace mikoto::renderer
