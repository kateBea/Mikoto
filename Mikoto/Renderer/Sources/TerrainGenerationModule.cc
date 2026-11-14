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

#include <limits>
#include <cstring>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/TerrainGenerationModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    static constexpr core::u32 kTerrainNoiseSize{ 1024 };

    // Matches TerrainBuildDescription in TerrainGeometry_Comp.slang.
    struct TerrainBuildDescription {
        core::float2 mOrigin{};
        core::f32 mVertexSpacing{};
        core::f32 mHeightScale{};
        core::f32 mBaseElevation{};
        core::u32 mColumns{};
        core::u32 mRows{};
        core::u32 mFirstVertex{};
        core::u32 mFirstIndex{};
        core::u32 mIndexCount{};
        core::u32 mHeightmapWidth{};
        core::u32 mHeightmapHeight{};
        core::u64 mHeightmapOffset{};
        // 0: flat, 1: RGBA8 pixels, 2: RGBA32F pixels, 3: generated GPU texture.
        core::u32 mHeightmapSource{};
        core::u32 mPadding{};
    };

    struct TerrainNoiseParameters {
        TerrainNoiseSettings mSettings{};
        core::u32 mPadding[2]{};
    };

    struct TerrainElevationColors {
        core::float4 mLow{};
        core::float4 mMiddle{};
        core::float4 mHigh{};
    };

    static auto SameTerrainGeometry( const WorldDescription& a, const WorldDescription& b ) -> bool {
        return a.mOrigin == b.mOrigin && a.mQuadsPerSection == b.mQuadsPerSection &&
               a.mSectionsPerComponent == b.mSectionsPerComponent && a.mComponentsX == b.mComponentsX &&
               a.mComponentsY == b.mComponentsY && a.mVertexSpacing == b.mVertexSpacing &&
               a.mHeightScale == b.mHeightScale && a.mBaseElevation == b.mBaseElevation;
    }

    // Matches TerrainDebugFlag in TerrainRender_Helpers.slang.
    enum class TerrainDebugFlag : core::u32 {
        eNone = 0,
        eGridLines = BIT_SET( 0 ),
        eVertices = BIT_SET( 1 )
    };

    // One float4 per vertex: local XYZ and the normalized height used for debug color.
    // Parameters match TerrainPlane_Vert.slang; matrices follow the existing water path.
    struct TerrainParameters {
        core::float4x4 mWorld{};
        SPointer mVertices{};
        SPointer mIndices{};
        SPointer mCamera{};

        core::u32 mFirstVertex{};
        core::u32 mFirstIndex{};
        core::u32 mColumns{};
        core::u32 mDebugFlags{};
        SPointer mElevationColors{};
    };

    TerrainGenerationModule::TerrainGenerationModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto TerrainGenerationModule::SetScene( const scene::Scene* scene ) -> void {
        if ( mScene != scene ) {
            mTerrains.clear();
            mGeometryDirty = true;
            mColorsDirty = true;
            mBudgetWarning = false;
            // An in-flight copy still owns the scratch readback buffer. Let its
            // callback finish before servicing requests from the new scene.
        }

        mScene = scene;
    }

    auto TerrainGenerationModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto TerrainGenerationModule::RegisterTerrainGeneration( FrameGraph& graph ) -> void {
        mTerrains.clear();
        mPendingNoise.reset();
        mLastGpuNoise.reset();
        mNoiseReadbackComplete = false;

        mColorsDirty = true;
        mGeometryDirty = true;
        mNoiseSubmitted = false;

        auto& info{ graph.GetOrCreate<TerrainGenerationInfo>() };
        const auto usage{
            rhi::BufferUsageFlagsBits::Structured |
            rhi::BufferUsageFlagsBits::CopySource |
            rhi::BufferUsageFlagsBits::CopyDest
        };

        info.mVertices = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_Vertices" )
            .SetSizeBytes( sizeof( core::float4 ) )
            .SetUsage( usage )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mIndices = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_Indices" )
            .SetSizeBytes( sizeof( core::u32 ) )
            .SetUsage( usage )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mBuildParameters = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_BuildParameters" )
            .SetSizeBytes( sizeof( TerrainBuildDescription ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mHeightmapPixels = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_HeightmapPixels" )
            .SetSizeBytes( 4 )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mNoiseParameters = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_NoiseParameters" )
            .SetSizeBytes( sizeof( TerrainNoiseParameters ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mElevationColors = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_ElevationColors" )
            .SetSizeBytes( sizeof( TerrainElevationColors ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mGeometryPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Terrain_GeometryCompute" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "TerrainGeometry_Comp.slang", FGStageType::eCompute ) );

        const auto declareInputs{ []( FGNodeBuilder& builder, core::Blackboard& b ) {
            const auto& info{ b.Get<TerrainGenerationInfo>() };
            builder.UseResource( info.mBuildParameters, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( info.mHeightmapPixels, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( info.mNoiseParameters, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( info.mElevationColors, FGPipelineStage::eCopy, FGResourceAccess::eWrite );

            // Preparation may replace storage; upload/compute use the new buffers.
            builder.UseResource( info.mVertices, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( info.mIndices, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
        } };

        graph.RegisterPass(
            "Terrain_FilterTerrains",
            FGPassType::eTransfer,
            declareInputs,
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                mUploads.Clear();
                FilterTerrains();
                PrepareGeometryInputs( ctx, b );
            } );

        graph.RegisterPass(
            "Terrain_UploadGeometryInputs",
            FGPassType::eTransfer,
            declareInputs,
            [this]( CommandContext& ctx, core::Blackboard& ) {
                mUploads.Submit( ctx );
            } );

        RegisterNoiseGeneration( graph );

        graph.RegisterPass(
            "Terrain_GenGeometry",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& info{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( info.mBuildParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mHeightmapPixels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mNoiseTarget, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mVertices, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( info.mIndices, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                GenerateGeometry( ctx, b );
            } );
    }

    auto TerrainGenerationModule::RegisterNoiseGeneration( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<TerrainGenerationInfo>() };

        // One scratch target/readback services requests in turn. Each completed
        // request retains its own image; different landscapes never share output.
        info.mNoiseTarget = graph.Create( FGTextureDescription{}
            .SetName( "Terrain_NoiseTexture" )
            .SetWidth( kTerrainNoiseSize )
            .SetHeight( kTerrainNoiseSize )
            .SetDimensions( rhi::TextureDimension::eTexture2D )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | 
                rhi::TextureUsageFlagsBits::CopySource | 
                rhi::TextureUsageFlagsBits::ShaderResource ) );

        info.mNoiseReadback = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_NoiseReadback" )
            .SetSizeBytes( static_cast<core::usize>( kTerrainNoiseSize ) * kTerrainNoiseSize * sizeof( core::float4 ) )
            .SetHeapType( rhi::HeapType::eReadback )
            .SetUsage( rhi::BufferUsageFlagsBits::CopyDest ) );

        info.mNoisePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Terrain_NoisePipeline" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleStrip )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eRGBA32_FLOAT )
            .PushShader( "Noise_Vert.slang", FGStageType::eVertex )
            .PushShader( "TerrainNoise_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "Terrain_NoiseGenerate",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<TerrainGenerationInfo>().mNoiseParameters, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<TerrainGenerationInfo>().mNoiseTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                if ( mNoiseSubmitted || mPendingNoise.expired() ) {
                    return;
                }

                const auto& data{ b.Get<TerrainGenerationInfo>() };
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ kTerrainNoiseSize, kTerrainNoiseSize } )
                    .AddRenderTarget( data.mNoiseTarget, rhi::kColorBlack, rhi::LoadOp::eClear ) );

                ctx.SetViewportState( rhi::ViewportState{}
                    .AddViewportAndScissorRect( rhi::Viewport( kTerrainNoiseSize, kTerrainNoiseSize ) ) );
                
                ctx.BindPipeline( data.mNoisePipeline );
                ctx.PushConstants( ctx.GetDeviceBufferAddress( data.mNoiseParameters ) );
                
                ctx.Draw( 4 );
                
                ctx.EndRender();

                mLastGpuNoise = mPendingNoise;
            } );

        graph.RegisterPass(
            "Terrain_NoiseReadback",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& data{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( data.mNoiseTarget, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( data.mNoiseReadback, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, &graph]( CommandContext& ctx, core::Blackboard& b ) {
                const auto request{ mPendingNoise.lock() };

                if ( mNoiseSubmitted || !request ) {
                    return;
                }

                const auto& data{ b.Get<TerrainGenerationInfo>() };
                
                ctx.Copy( data.mNoiseReadback, data.mNoiseTarget );
                mNoiseReadbackComplete = false;

                // The renderer destroys the graph before this module. The callback runs
                // on the render thread and tracks this view's copy of the shared request.
                graph.RegisterReadback(
                    [request, &completed = mNoiseReadbackComplete,
                        buffer = data.mNoiseReadback]( core::Blackboard&, const FGResourceManager& resources ) {
                        completed = true;

                        try {
                            const void* pixels{ resources.GetBufferMappedAddress( buffer ) };

                            if ( !pixels ) {
                                request->Fail( "Generated heightmap readback failed." );
                                return;
                            }

                            auto image{ asset::ImageHandle::New() };
                            image->mWidth = kTerrainNoiseSize;
                            image->mHeight = kTerrainNoiseSize;
                            image->mChannels = 4;
                            image->mFormat = asset::ImageFormat::eRGBA32_FLOAT;
                            image->mBufferSpan = memory::BufferSpanHandle::New( pixels,
                                static_cast<core::usize>( kTerrainNoiseSize ) * kTerrainNoiseSize * sizeof( core::float4 ) );
                            request->Complete( eastl::move( image ) );
                        } catch ( const std::exception& error ) {
                            request->Fail( error.what() );
                        }
                    } );

                // Do not reuse the scratch readback buffer until its CPU callback
                // has copied the pixels. Future edits queue a distinct request.
                mNoiseSubmitted = true;
            } );
    }

    auto TerrainGenerationModule::FilterTerrains() -> void {
        const auto pending{ mPendingNoise.lock() };
        const bool readbackComplete{ mNoiseSubmitted && mNoiseReadbackComplete };
        const bool alreadyGenerated{ !mNoiseSubmitted && pending &&
            ( pending->GetHeightmap() || !pending->GetError().empty() ) };

        if ( readbackComplete || alreadyGenerated ) {
            mPendingNoise.reset();
            mNoiseSubmitted = false;
        }

        bool overBudget{};
        core::u64 indexCount{};
        core::u64 vertexCount{};
        eastl::vector<Terrain> terrains{};

        if ( mScene ) {
            for ( const auto [entity, landscape, tag, transform]:
                mScene->GetRegistry().view<scene::LandscapeComponent, scene::TagComponent, scene::TransformComponent>().each() ) {

                const auto noise{ landscape.GetNoiseGeneration() };
                // Generate previews even for temporarily hidden terrain entities.
                if ( !mNoiseSubmitted && mPendingNoise.expired() &&
                     noise && !noise->GetHeightmap() && noise->GetError().empty() ) {
                    mPendingNoise = noise;
                    mGeometryDirty = true;
                }

                if ( !tag.IsActive() ) {
                    continue;
                }

                const auto world{ landscape.GetWorldDescription() };

                if ( !IsTerrainDescriptionValid( world ) ) {
                    continue;
                }

                const core::u64 vertices{ static_cast<core::u64>( world.GetResolutionX() ) * world.GetResolutionY() };
                const core::u64 indices{ static_cast<core::u64>( world.GetResolutionX() - 1 ) * ( world.GetResolutionY() - 1 ) * 6 };

                // This first implementation is fully resident, without terrain streaming/LOD.
                if ( vertexCount + vertices > 8388608 ) {
                    overBudget = true;
                    continue;
                }

                const auto index{ terrains.size() };
                const auto heightmap{ landscape.GetHeightmap() };

                if ( index >= mTerrains.size() || mTerrains[index].mEntity != entity ) {
                    mGeometryDirty = mColorsDirty = true;
                }
                else if ( !landscape.IsRevision( mTerrains[index].mRevision ) ) {
                    const auto& previous{ mTerrains[index] };
                    mGeometryDirty |= !SameTerrainGeometry( previous.mWorld, world ) ||
                                      previous.mHeightmap != heightmap || previous.mNoise != noise;
                    mColorsDirty |= previous.mWorld.mLowColor != world.mLowColor ||
                                    previous.mWorld.mMiddleColor != world.mMiddleColor || previous.mWorld.mHighColor != world.mHighColor;
                }

                terrains.push_back( Terrain{ entity, landscape.GetRevision(), static_cast<core::u32>( vertexCount ),
                    static_cast<core::u32>( indexCount ), static_cast<core::u32>( indices ), noise, world, heightmap } );
                vertexCount += vertices;
                indexCount += indices;
            }
        }

        if ( overBudget && !mBudgetWarning ) {
            MKT_CORE_LOGGER_WARN( "Terrain preview exceeds its combined 8M vertex budget; excess landscapes are skipped. Reduce component counts or section size." );
        }

        mBudgetWarning = overBudget;
        mGeometryDirty |= terrains.size() != mTerrains.size();
        mColorsDirty |= terrains.size() != mTerrains.size();
        mTerrains = eastl::move( terrains );
    }

    auto TerrainGenerationModule::PrepareGeometryInputs( CommandContext& ctx, core::Blackboard& b ) -> void {
        const auto& info{ b.Get<TerrainGenerationInfo>() };

        if ( const auto request{ mPendingNoise.lock() }; request && !mNoiseSubmitted ) {
            const TerrainNoiseParameters noise{ request->GetSettings() };
            mUploads.CopyBuffer( info.mNoiseParameters, 0, &noise, sizeof( noise ) );
        }

        if ( mColorsDirty && !mTerrains.empty() ) {
            eastl::vector<TerrainElevationColors> colors{};
            colors.reserve( mTerrains.size() );

            for ( const auto& terrain: mTerrains ) {
                colors.push_back( TerrainElevationColors{ core::float4{ terrain.mWorld.mLowColor, 1.0f },
                    core::float4{ terrain.mWorld.mMiddleColor, 1.0f }, core::float4{ terrain.mWorld.mHighColor, 1.0f } } );
            }

            ctx.EnsureBufferCapacity( info.mElevationColors, colors.size() * sizeof( TerrainElevationColors ) );

            mUploads.CopyBuffer( info.mElevationColors, 0, colors.data(), colors.size() * sizeof( TerrainElevationColors ) );
            mColorsDirty = false;
        }

        if ( !mGeometryDirty || !mScene || mTerrains.empty() ) {
            return;
        }

        eastl::vector<TerrainBuildDescription> descriptions{};
        eastl::vector<core::ubyte> pixels{};

        // The next draw may replace the shared scratch texture. Only that request
        // can sample it directly; other maps use their retained image pixels.
        const auto pending{ mPendingNoise.lock() };
        const auto gpuNoise{ pending && !mNoiseSubmitted ? pending : mLastGpuNoise.lock() };

        for ( const auto& terrain: mTerrains ) {
            const auto& landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( terrain.mEntity ) };
            const auto world{ landscape.GetWorldDescription() };
            TerrainBuildDescription description{
                .mOrigin = world.mOrigin,
                .mVertexSpacing = world.mVertexSpacing,
                .mHeightScale = world.mHeightScale,
                .mBaseElevation = world.mBaseElevation,
                .mColumns = world.GetResolutionX(),
                .mRows = world.GetResolutionY(),
                .mFirstVertex = terrain.mFirstVertex,
                .mFirstIndex = terrain.mFirstIndex,
                .mIndexCount = terrain.mIndexCount,
            };

            if ( terrain.mNoise && terrain.mNoise == gpuNoise ) {
                // Consume newly rendered noise on the GPU in this same execution;
                // its asynchronous CPU readback is for preview/export and retention.
                description.mHeightmapSource = 3;
                description.mHeightmapWidth = description.mHeightmapHeight = kTerrainNoiseSize;
            }
            else if ( const auto heightmap{ landscape.GetHeightmap() } ) {
                const auto& image{ heightmap->mImage };

                description.mHeightmapSource = image->mFormat == asset::ImageFormat::eRGBA32_FLOAT ? 2u : 1u;
                description.mHeightmapWidth = heightmap->mWidth;
                description.mHeightmapHeight = heightmap->mHeight;
                const auto bytes{ image->mBufferSpan->GetSize() };

                // float4 buffer references require 16-byte-aligned image starts.
                const auto offset{ ( pixels.size() + 15 ) & ~core::usize{ 15 } };
                
                // Shared buffer references use 32-bit byte offsets on D3D12.
                constexpr core::usize maxBytes{ std::numeric_limits<core::u32>::max() };

                if ( offset > maxBytes || bytes > maxBytes - offset ) {
                    MKT_THROW_RUNTIME_ERROR( "Terrain heightmap pixel batch exceeds the shared 32-bit addressing limit." );
                }

                description.mHeightmapOffset = offset;
                pixels.resize( offset + bytes );
                std::memcpy( pixels.data() + offset, image->mBufferSpan->GetData(), bytes );
            }

            descriptions.push_back( description );
        }

        const auto& last{ descriptions.back() };
        const core::usize vertices{ static_cast<core::usize>( last.mFirstVertex ) + static_cast<core::usize>( last.mColumns ) * last.mRows };
        const core::usize indices{ static_cast<core::usize>( last.mFirstIndex ) + last.mIndexCount };

        ctx.EnsureBufferCapacity( info.mVertices, vertices * sizeof( core::float4 ) );
        ctx.EnsureBufferCapacity( info.mIndices, indices * sizeof( core::u32 ) );
        ctx.EnsureBufferCapacity( info.mBuildParameters, descriptions.size() * sizeof( TerrainBuildDescription ) );
        ctx.EnsureBufferCapacity( info.mHeightmapPixels, pixels.size() );
        
        mUploads.CopyBuffer( info.mBuildParameters, 0, descriptions.data(), descriptions.size() * sizeof( TerrainBuildDescription ) );

        if ( !pixels.empty() ) {
            mUploads.CopyBuffer( info.mHeightmapPixels, 0, pixels.data(), pixels.size() );
        }
    }

    auto TerrainGenerationModule::GenerateGeometry( CommandContext& ctx, core::Blackboard& b ) -> void {
        if ( !mGeometryDirty || !mScene || mTerrains.empty() ) {
            return;
        }

        const auto& info{ b.Get<TerrainGenerationInfo>() };
        struct Parameters {
            SPointer mVertices{};
            SPointer mIndices{};
            SPointer mDescriptions{};
            SPointer mHeightmapPixels{};
            core::u32 mTerrain{};
            core::u32 mNoiseTexture{};
        } parameters{
            .mVertices = ctx.GetDeviceBufferAddress( info.mVertices ),
            .mIndices = ctx.GetDeviceBufferAddress( info.mIndices ),
            .mDescriptions = ctx.GetDeviceBufferAddress( info.mBuildParameters ),
            .mHeightmapPixels = ctx.GetDeviceBufferAddress( info.mHeightmapPixels ),
            .mNoiseTexture = ctx.PushTexture_SRV( info.mNoiseTarget ),
        };
        ctx.BindPipeline( info.mGeometryPipeline );

        for ( core::u32 index{}; index < mTerrains.size(); ++index ) {
            const auto& landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( mTerrains[index].mEntity ) };
            const auto world{ landscape.GetWorldDescription() };
            const core::u32 count{ world.GetResolutionX() * world.GetResolutionY() };
            parameters.mTerrain = index;
            ctx.PushConstants( parameters );
            ctx.Dispatch( ( count + 127 ) / 128, 1, 1 );
        }

        mGeometryDirty = false;
    }

    auto TerrainGenerationModule::RegisterTerrainRendering( FrameGraph& graph, bool hdr ) -> void {
        auto& info{ graph.GetOrCreate<TerrainGenerationInfo>() };
        const auto color{ hdr ? graph.GetData<GeomShadingModuleInfo>().mColorImage : graph.GetData<PresentationPassData>().mColorImage };

        info.mPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Terrain_Plane" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .SetBlendEnable( false )
            .AddColorFormat( hdr ? rhi::Format::eRGBA16_FLOAT : rhi::Format::eBGRA8_UNORM )
            .PushShader( "TerrainPlane_Vert.slang", FGStageType::eVertex )
            .PushShader( "TerrainPlane_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "Terrain_RenderGeometry",
            FGPassType::eGraphics,
            [color]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& info{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( info.mVertices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( info.mIndices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( info.mElevationColors, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( color, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, color]( CommandContext& ctx, core::Blackboard& b ) {
                if ( !mScene || !mCamera || mTerrains.empty() ) {
                    return;
                }

                const auto& info{ b.Get<TerrainGenerationInfo>() };
                const auto [width, height]{ rhi::InferDimensions( mResolution ) };
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ static_cast<core::i32>( width ), static_cast<core::i32>( height ) } )
                    .AddRenderTarget( color, rhi::Color{}, rhi::LoadOp::eLoad )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( rhi::ViewportState{}
                    .AddViewportAndScissorRect( rhi::Viewport( as<float>( width ), as<float>( height ) ) ) );

                ctx.BindPipeline( info.mPipeline );

                TerrainParameters parameters{};
                parameters.mVertices = ctx.GetDeviceBufferAddress( info.mVertices );
                parameters.mIndices = ctx.GetDeviceBufferAddress( info.mIndices );
                parameters.mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData );

                core::usize paletteIndex{};
                const auto colorAddress{ ctx.GetDeviceBufferAddress( info.mElevationColors ) };

                for ( const auto& terrain: mTerrains ) {
                    const auto& landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( terrain.mEntity ) };
                    parameters.mWorld = mScene->GetRegistry().get<scene::TransformComponent>( terrain.mEntity ).GetWorldTransform();
                    parameters.mFirstVertex = terrain.mFirstVertex;
                    parameters.mFirstIndex = terrain.mFirstIndex;
                    parameters.mColumns = landscape.GetWorldDescription().GetResolutionX();
                    const auto lines{ landscape.IsGridVisible() ? TerrainDebugFlag::eGridLines : TerrainDebugFlag::eNone };
                    const auto vertices{ landscape.AreVerticesVisible() ? TerrainDebugFlag::eVertices : TerrainDebugFlag::eNone };
                    parameters.mDebugFlags = core::as<core::u32>( lines ) | core::as<core::u32>( vertices );
                    parameters.mElevationColors = colorAddress + paletteIndex++ * sizeof( TerrainElevationColors );

                    ctx.PushConstants( parameters );
                    ctx.Draw( terrain.mIndexCount );
                }

                ctx.EndRender();
            } );
    }
}// namespace mikoto::renderer
