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

// MIT License
//
// Copyright (c) 2026 ケイト
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <random>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CpuPathTraceModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    constexpr core::u32 kTileSize{ 64 };

    // Kept in sync with Sphere in raytracing/SoftwarePathtracer.slang.
    struct SphereGPU {
        core::float4 mCenterRadius{};
        core::float4 mAlbedoParameter{};// RGB, then fuzz or refractive index.
        core::u32 mMaterial{};          // Lambertian, metal, dielectric.
        core::u32 mPadding[3]{};
    };

    MKT_NODISCARD static auto CreateScene() -> eastl::vector<SphereGPU> {
        // The same default-seeded generator and scene recipe as main.cc.
        std::mt19937 generator{};
        std::uniform_real_distribution<double> distribution{ 0, 1 };
        const auto random{ [&] {
            return distribution( generator );
        } };

        const auto randomColor{ [&] {
            return core::float3{ random(), random(), random() };
        } };

        eastl::vector<SphereGPU> spheres{};
        const auto add{ [&]( core::float3 center, float radius, core::float3 albedo, float parameter, core::u32 material ) {
            spheres.push_back( { core::float4{ center, radius }, core::float4{ albedo, parameter }, material } );
        } };

        add( { 0, -1000, 0 }, 1000, { 0.5f, 0.5f, 0.5f }, 0, 0 );

        for ( int a{ -11 }; a < 11; ++a ) {
            for ( int b{ -11 }; b < 11; ++b ) {
                const double chooseMaterial{ random() };
                const core::float3 center{ a + 0.9 * random(), 0.2, b + 0.9 * random() };

                if ( glm::length( center - core::float3{ 4, 0.2f, 0 } ) <= 0.9f ) {
                    continue;
                }

                if ( chooseMaterial < 0.8 ) {
                    const auto first{ randomColor() };
                    const auto second{ randomColor() };
                    add( center, 0.2f, first * second, 0, 0 );
                } else if ( chooseMaterial < 0.95 ) {
                    const auto albedo{ core::float3{ 0.5f } + 0.5f * randomColor() };
                    const float fuzz{ static_cast<float>( 0.5 * random() ) };
                    add( center, 0.2f, albedo, fuzz, 1 );
                } else {
                    add( center, 0.2f, core::float3{ 1 }, 1.5f, 2 );
                }
            }
        }

        add( { 0, 1, 0 }, 1, core::float3{ 1 }, 1.5f, 2 );
        add( { -4, 1, 0 }, 1, { 0.4f, 0.2f, 0.1f }, 0, 0 );
        add( { 4, 1, 0 }, 1, { 0.7f, 0.6f, 0.5f }, 0, 1 );

        return spheres;
    }

    CpuPathTraceModule::CpuPathTraceModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto CpuPathTraceModule::SetUseComputeApproach( bool enable ) -> void {
        mUseComputeApproach = enable;
    }

    auto CpuPathTraceModule::SetRenderPerTile( bool enable ) -> void {
        mRenderPerTile = enable;
    }

    auto CpuPathTraceModule::RegisterPathTracerCompute( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        mNextTile = 0;
        mSceneUploaded = false;
        mImageCleared = false;
        mTraceRecorded = false;
        mReadbackPending = false;

        auto& info{ graph.GetOrCreate<CpuPathTraceModuleInfo>() };
        const auto dimensions{ rhi::InferDimensions( mResolution ) };
        const core::u32 width{ static_cast<core::u32>( dimensions.first ) };
        const core::u32 height{ static_cast<core::u32>( dimensions.second ) };
        auto spheres{ CreateScene() };
        const core::u32 sphereCount{ static_cast<core::u32>( spheres.size() ) };

        info.mColorImage = graph.Create( FGTextureDescription{}
            .SetName( "CpuTraceRays_ColorImage01" )
            .SetWidth( width )
            .SetHeight( height )
            .SetDimensions( rhi::TextureDimension::eTexture2D )
            .SetMultisampling( rhi::Multisampling::eMsaaX1 )
            .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::CopySource | rhi::TextureUsageFlagsBits::UnorderedAccess )
            .SetFormat( rhi::Format::eRGBA8_UNORM ) );

        info.mSpheres = graph.Create( FGBufferDescription{}
            .SetName( "CpuTraceRays_Spheres" )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( spheres.size() * sizeof( SphereGPU ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mReadbackBuffer = graph.Create( FGBufferDescription{}
            .SetName( "CpuTraceRays_Readback" )
            .SetUsage( rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( static_cast<core::usize>( width ) * height * 4 )
            .SetHeapType( rhi::HeapType::eReadback ) );

        info.mComputePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "CpuTraceRays_ComputePipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "CpuTraceRay_Comp.slang", FGStageType::eCompute ) );

        graph.RegisterPass(
            "SoftwarePathTrace_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<CpuPathTraceModuleInfo>().mSpheres, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, spheres = eastl::move( spheres )]( CommandContext& ctx, core::Blackboard& b ) {
                if ( mUseComputeApproach && !mSceneUploaded ) {
                    ctx.CopyBuffer( b.Get<CpuPathTraceModuleInfo>().mSpheres, 0, spheres.data(), MKT_VECTOR_SIZE_BYTES( spheres ) );
                    mSceneUploaded = true;
                }
            } );

        graph.RegisterPass(
            "SoftwarePathTrace_Compute",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& data{ b.Get<CpuPathTraceModuleInfo>() };
                builder.UseResource( data.mSpheres, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mColorImage, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, width, height, sphereCount]( CommandContext& ctx, core::Blackboard& b ) {
                if ( !mUseComputeApproach || mTraceRecorded ) {
                    return;
                }

                const core::u32 tilesX{ ( width + kTileSize - 1 ) / kTileSize };
                const core::u32 tileCount{ tilesX * ( ( height + kTileSize - 1 ) / kTileSize ) };
                const bool clearImage{ mRenderPerTile && !mImageCleared };
                const auto& data{ b.Get<CpuPathTraceModuleInfo>() };

                struct Parameters {
                    SPointer mSpheres{};
                    core::u32 mImageIndex{};
                    core::u32 mSphereCount{};
                    core::u32 mTileX{};
                    core::u32 mTileY{};
                } params{
                    .mSpheres = ctx.GetDeviceBufferAddress( data.mSpheres ),
                    .mImageIndex = ctx.PushTexture_UAV( data.mColorImage ),
                    .mSphereCount = clearImage ? 0 : sphereCount,
                    .mTileX = mRenderPerTile ? ( mNextTile % tilesX ) * kTileSize : 0,
                    .mTileY = mRenderPerTile ? ( mNextTile / tilesX ) * kTileSize : 0,
                };

                ctx.BindPipeline( data.mComputePipeline );
                ctx.PushConstants( params );

                if ( clearImage || !mRenderPerTile ) {
                    ctx.Dispatch( ( width + 15 ) / 16, ( height + 15 ) / 16, 1 );
                } else {
                    ctx.Dispatch( kTileSize / 16, kTileSize / 16, 1 );
                }

                if ( clearImage ) {
                    mImageCleared = true;
                } else {
                    mTraceRecorded = !mRenderPerTile || ++mNextTile == tileCount;
                    mReadbackPending = mTraceRecorded;
                }
            } );

        graph.RegisterPass(
            "SoftwarePathTrace_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& data{ b.Get<CpuPathTraceModuleInfo>() };
                builder.UseResource( data.mColorImage, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( data.mReadbackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, &graph, width, height]( CommandContext& ctx, core::Blackboard& b ) {
                if ( !mReadbackPending ) {
                    return;
                }

                const auto& data{ b.Get<CpuPathTraceModuleInfo>() };
                ctx.Copy( data.mReadbackBuffer, data.mColorImage );

                // The graph orders this copy after compute. CPU access waits for
                // the fence of the submission containing both dispatch and copy.
                graph.RegisterReadback( [width, height]( core::Blackboard& blackboard, const FGResourceManager& manager ) {
                    const auto buffer{ blackboard.Get<CpuPathTraceModuleInfo>().mReadbackBuffer };

                    if ( const auto* pixels{ manager.GetBufferMappedAddress( buffer ) } ) {
                        asset::WriteImage( "Resources/TraceRays.png", pixels, width, height, asset::ImageFormat::eRGBA8_UINT );
                    }
                } );

                mReadbackPending = false;
            } );
    }
}
