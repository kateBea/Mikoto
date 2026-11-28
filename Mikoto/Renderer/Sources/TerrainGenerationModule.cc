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

#include <cmath>
#include <cstring>
#include <limits>

#include <EASTL/algorithm.h>

#include <Assets/AssetRegistryService.hh>

#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Types.hh>

#include <Logging/Logger.hh>

#include <Material/SkyboxMaterial.hh>

#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/CloudsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/ForwardRenderer/Passes/TerrainGenerationModule.hh>
#include <Renderer/ForwardRenderer/Passes/WeatherModule.hh>
#include <Renderer/Rhi/Utility.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    static constexpr core::u32 kTerrainNoiseSize{ 1024 };

    // GrassFlow's placement, cubic deformation and shading are adapted to
    // Mikoto's generated terrain, bindless resources and shared wind volumes.
    // https://github.com/Mithzzx/Project-GrassFlow (MIT; see THIRD_PARTY.md).
    // These layouts mirror base/Grass_Helpers.slang on Vulkan and D3D12.
    // Tile subdivision and 15/7-vertex strips follow the geometry LOD approach
    // in Eric Wohllaib's GDC talk, Procedural Grass in Ghost of Tsushima:
    // https://www.youtube.com/watch?v=Ibe1JBF5i5Y
    // The hierarchical rank used to hide tile transitions is Mikoto-specific.
    // Each leaf covers a world-anchored square with the same candidate count.
    // Larger leaves visit a nested subset of the fine grid, without relocating
    // roots when the camera moves. CPU/GPU layouts are mirrored in Grass_Helpers.
    struct GrassTile {
        glm::ivec2 mCellOrigin{};
        core::u32 mStride{};
        core::u32 mLevel{};
    };
    static_assert( sizeof( GrassTile ) == 16 );

    struct GrassTileSelectionDescription {
        core::float2 mMinimum{};
        core::float2 mMaximum{};
        core::float2 mCamera{};
        core::f32 mSpacing{};
        core::f32 mPlanarScale{};
        core::f32 mViewDistance{};
        core::f32 mDensityDistance{};
        core::f32 mRootDisplacement{};
    };

    struct GrassTileSelection {
        eastl::vector<GrassTile> mTiles{};
        core::f32 mCandidateWeight{};
    };

    MKT_NODISCARD static auto BuildGrassTiles( const GrassTileSelectionDescription &description ) -> GrassTileSelection {
        GrassTileSelection selection{};
        const float tileSize{ description.mSpacing * kGrassTileSide };
        const float radius{ description.mViewDistance / description.mPlanarScale };
        const float displacement{ description.mRootDisplacement };
        const auto minimum{ glm::max( description.mMinimum, description.mCamera - radius ) - displacement };
        const auto maximum{ glm::min( description.mMaximum, description.mCamera + radius ) + displacement };
        if ( glm::any( glm::lessThanEqual( maximum, minimum ) ) ) {
            return selection;
        }

        // Start from a few coarse, globally aligned roots and subdivide only
        // where the distance-dependent density needs finer candidate cells.
        const auto rootLevel{ glm::min( kGrassMaxTileLevel, static_cast<core::u32>( glm::ceil( glm::log2( glm::max( 1.0f, 2.0f * radius / tileSize ) ) ) ) ) };
        const float rootSize{ tileSize * ( 1u << rootLevel ) };
        const auto first{ glm::ivec2{ glm::floor( minimum / rootSize ) } };
        const auto last{ glm::ivec2{ glm::floor( maximum / rootSize ) } };
        const auto visit{ [&]( auto &&self, glm::ivec2 origin, core::u32 level ) -> void {
            const auto stride{ 1u << level };
            const auto lower{ core::float2{ origin } * description.mSpacing - displacement };
            const auto upper{ core::float2{ origin } * description.mSpacing + tileSize * stride + displacement };
            if ( glm::any( glm::lessThan( upper, minimum ) ) || glm::any( glm::greaterThan( lower, maximum ) ) ) {
                return;
            }
            const auto delta{ glm::max( glm::max( lower - description.mCamera, description.mCamera - upper ), core::float2{} ) };
            const float nearest{ glm::length( delta ) * description.mPlanarScale };
            if ( nearest >= description.mViewDistance ) {
                return;
            }
            if ( level > 0 && nearest < description.mDensityDistance * stride ) {
                const auto half{ static_cast<core::i32>( kGrassTileSide * ( stride >> 1 ) ) };
                for ( core::i32 y{}; y < 2; ++y ) {
                    for ( core::i32 x{}; x < 2; ++x ) {
                        self( self, origin + glm::ivec2{ x, y } * half, level - 1 );
                    }
                }
                return;
            }

            selection.mTiles.push_back( GrassTile{
                .mCellOrigin = origin,
                .mStride = stride,
                .mLevel = level,
            } );
            // In a level-L tile all surviving ranks lie below 1 / 4^L.
            // This estimates the maximum survivors before paint/frustum tests;
            // it also keeps indirect instance reservations within their budget.
            const float ratio{ description.mDensityDistance / glm::max( nearest, description.mDensityDistance ) };
            selection.mCandidateWeight += kGrassTileCandidateCount * glm::min( 1.0f, ratio * ratio * stride * stride );
        } };
        const auto rootCells{ static_cast<core::i32>( kGrassTileSide * ( 1u << rootLevel ) ) };
        for ( core::i32 y{ first.y }; y <= last.y; ++y ) {
            for ( core::i32 x{ first.x }; x <= last.x; ++x ) {
                visit( visit, glm::ivec2{ x, y } * rootCells, rootLevel );
            }
        }
        return selection;
    }

    struct TerrainGrassDescription {
        core::float4x4 mWorld{ 1.0f };
        core::float4 mTiling{};
        core::float4 mShape{};
        core::float4 mView{};
        core::float4 mWind{};
        core::float4 mRootColor{};
        core::float4 mMidColor{};
        core::float4 mTipColor{};
        core::float4 mDryColor{};
        core::float4 mLighting{};
        core::float4 mDetail{};
        core::float4 mLimits{};
        core::float4 mBudgets{};
        core::float4 mShadows{};
        core::uint4 mGrid{};
        core::uint4 mRanges{};
        core::uint4 mControl{};
        core::uint4 mMesh{};
        core::float4 mMaterialColor{};
        core::float4 mMaterialOptions{};
        core::float4 mPalette{};
        SPointer mSurface{};
        SPointer mWindField{};
        SPointer mTiles{};
    };
    static_assert( sizeof( TerrainGrassDescription ) == 408 );

    struct TerrainGrassParameters {
        SPointer mDescriptions{};
        SPointer mVertices{};
        SPointer mCamera{};
        SPointer mBlades{};
        SPointer mMeshes{};
        SPointer mHiZ{};
        core::u32 mArguments{};
        core::u32 mTerrain{};
        core::u32 mLod{};
        core::u32 mFinalize{};
        core::u32 mHiZWidth{};
        core::u32 mHiZHeight{};
        core::u32 mHiZLevels{};
        core::u32 mDispatchWidth{};
    };
    static_assert( sizeof( TerrainGrassParameters ) == 80 );

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

        // 0: flat,
        // 1: RGBA8 pixels,
        // 2: RGBA32F pixels,
        // 3: generated GPU texture.
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

    static auto SameTerrainGeometry( const WorldDescription &a, const WorldDescription &b ) -> bool {
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
        SPointer mSurface{};
    };

    struct TerrainLayerData {
        core::float4 mTintRoughness{};
        core::float4 mUVNormalSlope{};
        core::float4 mHeightRange{};
        core::uint4 mTextures{ 0xffffffffu };
    };

    struct TerrainSurfaceData {
        eastl::array<TerrainLayerData, kTerrainSurfaceLayerCount> mLayers{};
        core::float4 mOptions{};
        core::uint4 mWeights{};
        core::uint4 mEnvironment{ 0xffffffffu };
        core::float4 mLighting{};
        core::uint4 mControls{};
        core::u64 mWeightBuffer{};
        core::u64 mLightBuffer{};
        core::u64 mShadowBuffer{};
        core::u64 mReserved{};
    };

    TerrainGenerationModule::TerrainGenerationModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto TerrainGenerationModule::SetScene( const scene::Scene *scene ) -> void {
        if ( mScene != scene ) {
            mTerrains.clear();
            mLayerRequests.clear();
            mPaintSources.clear();
            mGeometryDirty = true;
            mColorsDirty = true;
            mBudgetWarning = false;
            // An in-flight copy still owns the scratch readback buffer. Let its
            // callback finish before servicing requests from the new scene.
        }

        mScene = scene;
    }

    auto TerrainGenerationModule::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;
    }

    auto TerrainGenerationModule::RegisterTerrainGeneration( FrameGraph &graph ) -> void {
        mTerrains.clear();
        mPendingNoise.reset();
        mLastGpuNoise.reset();
        mPaintSources.clear();
        mNoiseReadbackComplete = false;

        mColorsDirty = true;
        mGeometryDirty = true;
        mNoiseSubmitted = false;

        auto &info{ graph.GetOrCreate<TerrainGenerationInfo>() };
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

        info.mSamplingRecords = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_SamplingRecords" )
            .SetSizeBytes( sizeof( core::uint4 ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        for ( auto *handle: { &info.mHeightBounds, &info.mBoundsScratch } ) {
            *handle = graph.Create( FGBufferDescription{}
                .SetName( handle == &info.mHeightBounds ? "Terrain_HeightBounds" : "Terrain_BoundsScratch" )
                .SetSizeBytes( sizeof( core::float4 ) * 2 )
                .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
                .SetHeapType( rhi::HeapType::eDeviceLocal ) );
        }

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

        info.mSurfaces = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_Surfaces" )
            .SetSizeBytes( sizeof( TerrainSurfaceData ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest | rhi::BufferUsageFlagsBits::CopySource ) );
        info.mPaintWeights = graph.Create( FGBufferDescription{}
            .SetName( "Terrain_PaintWeights" )
            .SetSizeBytes( sizeof( core::float4 ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest ) );
        info.mLayerSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Terrain_LayerSampler" )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eRepeat ) );

        info.mGeometryPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Terrain_GeometryCompute" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "TerrainGeometry_Comp.slang", FGStageType::eCompute ) );

        const auto declareInputs{ []( FGNodeBuilder &builder, core::Blackboard &b ) {
            const auto &info{ b.Get<TerrainGenerationInfo>() };
            builder.UseResource( info.mSamplingRecords, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( info.mHeightBounds, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( info.mBoundsScratch, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
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
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                mUploads.Clear();
                FilterTerrains();
                PrepareGeometryInputs( ctx, b );
            } );

        graph.RegisterPass(
            "Terrain_UploadGeometryInputs",
            FGPassType::eTransfer,
            declareInputs,
            [this]( CommandContext &ctx, core::Blackboard & ) {
                mUploads.Submit( ctx );
            } );

        RegisterNoiseGeneration( graph );

        graph.RegisterPass(
            "Terrain_GenGeometry",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &info{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( info.mBuildParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mHeightmapPixels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mNoiseTarget, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mVertices, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( info.mIndices, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                GenerateGeometry( ctx, b );
            } );

        mBoundsDirty = true;
        RegisterHeightBounds( graph );
    }
    
    auto TerrainGenerationModule::RegisterNoiseGeneration( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<TerrainGenerationInfo>() };

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
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( b.Get<TerrainGenerationInfo>().mNoiseParameters, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<TerrainGenerationInfo>().mNoiseTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                if ( mNoiseSubmitted || mPendingNoise.expired() ) {
                    return;
                }

                const auto &data{ b.Get<TerrainGenerationInfo>() };
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
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( data.mNoiseTarget, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( data.mNoiseReadback, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, &graph]( CommandContext &ctx, core::Blackboard &b ) {
                const auto request{ mPendingNoise.lock() };

                if ( mNoiseSubmitted || !request ) {
                    return;
                }

                const auto &data{ b.Get<TerrainGenerationInfo>() };

                ctx.Copy( data.mNoiseReadback, data.mNoiseTarget );
                mNoiseReadbackComplete = false;

                // The renderer destroys the graph before this module. The callback runs
                // on the render thread and tracks this view's copy of the shared request.
                graph.RegisterReadback(
                    [request, &completed = mNoiseReadbackComplete,
                        buffer = data.mNoiseReadback]( core::Blackboard &, const FGResourceManager &resources ) {
                        completed = true;

                        try {
                            const void *pixels{ resources.GetBufferMappedAddress( buffer ) };

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
                        } catch ( const std::exception &error ) {
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
                    const auto &previous{ mTerrains[index] };
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

    auto TerrainGenerationModule::RegisterHeightBounds( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Terrain_HeightBoundsPipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "TerrainBounds_Comp.slang", FGStageType::eCompute ) ) };
        for ( core::u32 phase{}; phase < 2; ++phase ) {
            graph.RegisterPass(
                phase == 0 ? "Pass_TerrainHeightBounds" : "Pass_TerrainHeightBoundsReduce",
                FGPassType::eCompute,
                [phase]( FGNodeBuilder &builder, core::Blackboard &b ) {
                    const auto &info{ b.Get<TerrainGenerationInfo>() };
                    builder.UseResource( info.mVertices, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( info.mSamplingRecords, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( info.mBoundsScratch, FGPipelineStage::eComputeShader,
                        phase == 0 ? FGResourceAccess::eWrite : FGResourceAccess::eRead );
                    if ( phase == 1 ) {
                        builder.UseResource( info.mHeightBounds, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                    }
                },
                [this, phase, pipeline]( CommandContext &ctx, core::Blackboard &b ) {
                    if ( !mBoundsDirty || mTerrains.empty() ) {
                        return;
                    }
                    const auto &info{ b.Get<TerrainGenerationInfo>() };
                    struct Parameters {
                        SPointer mVertices{};
                        SPointer mRecords{};
                        SPointer mScratch{};
                        SPointer mBounds{};
                        core::u32 mPhase{};
                    } parameters{
                        .mVertices = ctx.GetDeviceBufferAddress( info.mVertices ),
                        .mRecords = ctx.GetDeviceBufferAddress( info.mSamplingRecords ),
                        .mScratch = ctx.GetDeviceBufferAddress( info.mBoundsScratch ),
                        .mBounds = ctx.GetDeviceBufferAddress( info.mHeightBounds ),
                        .mPhase = phase,
                    };
                    ctx.BindPipeline( pipeline );
                    ctx.PushConstants( parameters );
                    ctx.Dispatch( phase == 0 ? 256u : static_cast<core::u32>( mTerrains.size() ),
                        phase == 0 ? static_cast<core::u32>( mTerrains.size() ) : 1u, 1 );
                    if ( phase == 1 ) {
                        mBoundsDirty = false;
                    }
                } );
        }
    }

    auto TerrainGenerationModule::PrepareGeometryInputs( CommandContext &ctx, core::Blackboard &b ) -> void {
        const auto &info{ b.Get<TerrainGenerationInfo>() };
        mBoundsDirty |= mGeometryDirty;
        const auto boundsBytes{ glm::max( core::usize{ 1 }, mTerrains.size() ) * sizeof( core::float4 ) * 2 };
        ctx.EnsureBufferCapacity( info.mHeightBounds, boundsBytes );
        ctx.EnsureBufferCapacity( info.mBoundsScratch, boundsBytes * 256 );

        if ( const auto request{ mPendingNoise.lock() }; request && !mNoiseSubmitted ) {
            const TerrainNoiseParameters noise{
                .mSettings = request->GetSettings(),
            };
            mUploads.Copy( info.mNoiseParameters, 0, &noise, sizeof( noise ) );
        }

        if ( mColorsDirty && !mTerrains.empty() ) {
            eastl::vector<TerrainElevationColors> colors{};
            colors.reserve( mTerrains.size() );

            for ( const auto &terrain: mTerrains ) {
                colors.push_back( TerrainElevationColors{ core::float4{ terrain.mWorld.mLowColor, 1.0f },
                    core::float4{ terrain.mWorld.mMiddleColor, 1.0f }, core::float4{ terrain.mWorld.mHighColor, 1.0f } } );
            }

            ctx.EnsureBufferCapacity( info.mElevationColors, colors.size() * sizeof( TerrainElevationColors ) );

            mUploads.Copy( info.mElevationColors, 0, colors.data(), colors.size() * sizeof( TerrainElevationColors ) );
            mColorsDirty = false;
        }

        // Publish the actual generated triangle grids, including live sculpt edits.
        // Consumers do not need a camera depth image or a duplicate CPU heightmap.
        struct SamplingRecord {
            core::float4x4 mWorld{};
            core::float4x4 mWorldToLocal{};
            core::uint4 mGrid{};
        };
        eastl::vector<SamplingRecord> sampling{};

        for ( const auto &terrain: mTerrains ) {
            const auto world{ mScene->GetRegistry().get<scene::TransformComponent>( terrain.mEntity ).GetWorldTransform() };
            sampling.push_back( { world, glm::inverse( world ),
                { terrain.mWorld.GetResolutionX(), terrain.mWorld.GetResolutionY(), terrain.mFirstVertex, 0 } } );
        }

        const core::uint4 count{ static_cast<core::u32>( sampling.size() ), 0, 0, 0 };
        ctx.EnsureBufferCapacity( info.mSamplingRecords, sizeof( count ) + sampling.size() * sizeof( SamplingRecord ) );
        mUploads.Copy( info.mSamplingRecords, 0, &count, sizeof( count ) );

        if ( !sampling.empty() ) {
            mUploads.Copy( info.mSamplingRecords, sizeof( count ), sampling.data(), sampling.size() * sizeof( SamplingRecord ) );
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

        for ( const auto &terrain: mTerrains ) {
            const auto &landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( terrain.mEntity ) };
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

            if ( !landscape.GetSculptedHeightmap() && terrain.mNoise && terrain.mNoise == gpuNoise ) {
                // Consume newly rendered noise on the GPU in this same execution;
                // its asynchronous CPU readback is for preview/export and retention.
                description.mHeightmapSource = 3;
                description.mHeightmapWidth = description.mHeightmapHeight = kTerrainNoiseSize;
            }
            else if ( const auto heightmap{ landscape.GetHeightmap() } ) {
                const auto &image{ heightmap->mImage };

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

        const auto &last{ descriptions.back() };
        const core::usize vertices{ static_cast<core::usize>( last.mFirstVertex ) + static_cast<core::usize>( last.mColumns ) * last.mRows };
        const core::usize indices{ static_cast<core::usize>( last.mFirstIndex ) + last.mIndexCount };

        ctx.EnsureBufferCapacity( info.mVertices, vertices * sizeof( core::float4 ) );
        ctx.EnsureBufferCapacity( info.mIndices, indices * sizeof( core::u32 ) );
        ctx.EnsureBufferCapacity( info.mBuildParameters, descriptions.size() * sizeof( TerrainBuildDescription ) );
        ctx.EnsureBufferCapacity( info.mHeightmapPixels, pixels.size() );

        mUploads.Copy( info.mBuildParameters, 0, descriptions.data(), descriptions.size() * sizeof( TerrainBuildDescription ) );

        if ( !pixels.empty() ) {
            mUploads.Copy( info.mHeightmapPixels, 0, pixels.data(), pixels.size() );
        }
    }

    auto TerrainGenerationModule::GenerateGeometry( CommandContext &ctx, core::Blackboard &b ) -> void {
        if ( !mGeometryDirty || !mScene || mTerrains.empty() ) {
            return;
        }

        const auto &info{ b.Get<TerrainGenerationInfo>() };
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
            .mNoiseTexture = ctx.PushSRV( info.mNoiseTarget ),
        };
        ctx.BindPipeline( info.mGeometryPipeline );

        for ( core::u32 index{}; index < mTerrains.size(); ++index ) {
            const auto &landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( mTerrains[index].mEntity ) };
            const auto world{ landscape.GetWorldDescription() };
            const core::u32 count{ world.GetResolutionX() * world.GetResolutionY() };
            parameters.mTerrain = index;
            ctx.PushConstants( parameters );
            ctx.Dispatch( ( count + 127 ) / 128, 1, 1 );
        }

        mGeometryDirty = false;
    }

    auto TerrainGenerationModule::UploadSurfaces( CommandContext &ctx, core::Blackboard &b, bool hdr ) -> void {
        if ( !mScene || mTerrains.empty() ) {
            return;
        }
        const auto &info{ b.Get<TerrainGenerationInfo>() };
        // The path-traced display overlay has no raster lights, shadow maps or IBL.
        const auto *lighting{ hdr ? &b.Get<GeomShadingModuleInfo>() : nullptr };
        const auto *lights{ hdr ? &b.Get<PrepassModuleInfo>() : nullptr };
        const auto *shadows{ hdr ? &b.Get<ShadowMapInfo>() : nullptr };
        const bool atmosphere{ mScene->GetEnvironmentLightingSource() == scene::EnvironmentLightingSource::eAtmosphere };
        const auto environment{ mScene->GetEnvironmentMaterial() };
        const bool ibl{ hdr && ( atmosphere || ( mScene->GetEnvironmentLightingSource() == scene::EnvironmentLightingSource::eEnvironmentMap && environment ) ) };
        eastl::vector<TerrainSurfaceData> surfaces{};
        eastl::vector<eastl::shared_ptr<const TerrainPaintMap>> paintSources{};
        eastl::vector<asset::AssetID> usedTextures{};
        core::u32 paintOffset{};

        const auto texture{ [&]( const asset::AssetRef<asset::Texture> &reference, bool *decodeSrgb = nullptr ) -> core::u32 {
            if ( reference.GetPath().IsEmpty() ) {
                return 0xffffffffu;
            }
            usedTextures.push_back( reference.GetID() );
            auto request{ eastl::find_if( mLayerRequests.begin(), mLayerRequests.end(), [&]( const auto &item ) {
                return item.mID == reference.GetID();
            } ) };
            if ( request == mLayerRequests.end() ) {
                mLayerRequests.push_back( { reference.GetID(), asset::AssetRegistryService::Get()->Request<asset::Texture>( reference ) } );
                request = mLayerRequests.end() - 1;
            }
            if ( const auto asset{ request->mHandle.GetIfReady() } ) {
                const auto resource{ asset->GetResource() };
                if ( resource && resource->GetDimension() == rhi::TextureDimension::eTexture2D ) {
                    const auto &format{ rhi::GetFormatInfo( resource->GetFormat() ) };
                    if ( decodeSrgb ) {
                        *decodeSrgb = !format.mIsSRGB && format.mKind != rhi::FormatKind::Float;
                    }
                    return ctx.UseImportedTexture( ctx.ImportSampledTexture( resource ) );
                }
            }

            return 0xffffffffu;
        } };

        for ( const auto &terrain: mTerrains ) {
            const auto &landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( terrain.mEntity ) };
            const auto &settings{ landscape.GetSurfaceSettings() };
            TerrainSurfaceData surface{};
            for ( core::u32 index{}; index < kTerrainSurfaceLayerCount; ++index ) {
                const auto &source{ settings.mLayers[index] };
                auto &target{ surface.mLayers[index] };
                bool decodeSrgb{};
                target.mTintRoughness = { source.mTint, source.mRoughness };
                target.mUVNormalSlope = { source.mUVScale, source.mNormalStrength, source.mSlopeInfluence };
                target.mHeightRange = { source.mHeightRange, 0, 0 };
                target.mTextures = { texture( source.mAlbedo, &decodeSrgb ), texture( source.mNormal ), texture( source.mHeight ), decodeSrgb ? 1u : 0u };
            }
            surface.mOptions = { settings.mBlendWidth, settings.mHeightBlendWidth, settings.mHeightBlendEnabled ? 1.0f : 0.0f, settings.mShowElevationColors ? 1.0f : 0.0f };
            const auto paint{ landscape.GetPaintMap() };
            paintSources.push_back( paint );
            if ( paint ) {
                surface.mWeights = { paint->mWidth, paint->mHeight, paintOffset, 0 };
                paintOffset += core::as<core::u32>( paint->mWeights.size() );
            }

            surface.mControls.x = ctx.PushSampler( info.mLayerSampler );
            surface.mControls.y = terrain.mWorld.GetResolutionY();
            if ( ibl ) {
                surface.mEnvironment = { ctx.PushSRV( lighting->mIrradianceCubeRT ),
                    ctx.PushSRV( atmosphere ? lighting->mAtmospherePrefilter : lighting->mPrefilterCubeRT ),
                    ctx.PushSampler( lighting->mSkyboxCubeSampler ), 6 };
            }

            surface.mLighting.w = hdr ? 0.0f : 1.0f;
            if ( hdr ) {
                surface.mLighting = { float( lights->mActiveLightCount ), ( atmosphere || ibl ) ? mScene->GetEnvironmentIntensity() : 0.0f,
                    float( shadows->mRecordCount ), 0 };
                surface.mLightBuffer = ctx.GetDeviceBufferAddress( lights->mLightsBuffer );
                surface.mShadowBuffer = ctx.GetDeviceBufferAddress( shadows->mRecordsBuffer );
            }

            surfaces.push_back( surface );
        }

        if ( paintSources != mPaintSources ) {
            eastl::vector<core::float4> weights{};
            for ( const auto &source: paintSources ) {
                if ( source ) {
                    weights.insert( weights.end(), source->mWeights.begin(), source->mWeights.end() );
                }
            }
            ctx.EnsureBufferCapacity( info.mPaintWeights, glm::max<core::usize>( sizeof( core::float4 ), weights.size() * sizeof( core::float4 ) ) );
            if ( !weights.empty() ) {
                ctx.Copy( info.mPaintWeights, 0, weights.data(), weights.size() * sizeof( core::float4 ) );
            }
            mPaintSources = eastl::move( paintSources );
        }

        for ( auto &surface: surfaces ) {
            surface.mWeightBuffer = ctx.GetDeviceBufferAddress( info.mPaintWeights );
        }

        ctx.EnsureBufferCapacity( info.mSurfaces, surfaces.size() * sizeof( TerrainSurfaceData ) );
        ctx.Copy( info.mSurfaces, 0, surfaces.data(), surfaces.size() * sizeof( TerrainSurfaceData ) );

        eastl::erase_if( mLayerRequests, [&]( const auto &request ) {
            return eastl::find( usedTextures.begin(), usedTextures.end(), request.mID ) == usedTextures.end();
        } );
    }

    auto TerrainGenerationModule::RegisterTerrainRendering( FrameGraph &graph, bool hdr ) -> void {
        auto &info{ graph.GetOrCreate<TerrainGenerationInfo>() };
        const auto color{ hdr ? graph.GetData<GeomShadingModuleInfo>().mColorImage : graph.GetData<PresentationPassData>().mColorImage };

        graph.RegisterPass(
            "Terrain_UploadSurfaces",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &info{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( info.mSurfaces, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( info.mPaintWeights, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, hdr]( CommandContext &ctx, core::Blackboard &b ) {
                UploadSurfaces( ctx, b, hdr );
            } );

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
            [color, hdr]( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &info{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( info.mVertices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( info.mIndices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( info.mElevationColors, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( info.mSurfaces, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( info.mSurfaces, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( info.mPaintWeights, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                if ( hdr ) {
                    // Register only resources produced by this rendering path.
                    const auto &lighting{ b.Get<GeomShadingModuleInfo>() };
                    for ( const auto texture: { lighting.mIrradianceCubeRT, lighting.mPrefilterCubeRT, lighting.mAtmospherePrefilter } ) {
                        builder.UseResource( texture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }

                    builder.UseResource( b.Get<PrepassModuleInfo>().mLightsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    const auto &shadows{ b.Get<ShadowMapInfo>() };
                    builder.UseResource( shadows.mRecordsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    for ( const auto map: shadows.mMaps ) {
                        builder.UseResource( map, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                    for ( const auto receiver: shadows.mGrassReceiverData ) {
                        if ( receiver.mHandle ) {
                            builder.UseResource( receiver, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        }
                    }

                    if ( const auto *clouds{ b.TryGet<CloudsModuleInfo>() } ) {
                        builder.UseResource( clouds->mFrame, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        builder.UseResource( clouds->mModels, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                        for ( const auto &volume: clouds->mVolumes ) {
                            builder.UseResource( volume.mLighting, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        }
                    }
                }

                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( color, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, color]( CommandContext &ctx, core::Blackboard &b ) {
                if ( !mScene || !mCamera || mTerrains.empty() ) {
                    return;
                }

                const auto &info{ b.Get<TerrainGenerationInfo>() };
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

                for ( const auto &terrain: mTerrains ) {
                    const auto &landscape{ mScene->GetRegistry().get<scene::LandscapeComponent>( terrain.mEntity ) };
                    parameters.mWorld = mScene->GetRegistry().get<scene::TransformComponent>( terrain.mEntity ).GetWorldTransform();
                    parameters.mFirstVertex = terrain.mFirstVertex;
                    parameters.mFirstIndex = terrain.mFirstIndex;
                    parameters.mColumns = landscape.GetWorldDescription().GetResolutionX();
                    const auto lines{ landscape.IsGridVisible() ? TerrainDebugFlag::eGridLines : TerrainDebugFlag::eNone };
                    const auto vertices{ landscape.AreVerticesVisible() ? TerrainDebugFlag::eVertices : TerrainDebugFlag::eNone };
                    parameters.mDebugFlags = core::as<core::u32>( lines ) | core::as<core::u32>( vertices );
                    parameters.mElevationColors = colorAddress + paletteIndex * sizeof( TerrainElevationColors );
                    parameters.mSurface = ctx.GetDeviceBufferAddress( info.mSurfaces ) + paletteIndex++ * sizeof( TerrainSurfaceData );

                    ctx.PushConstants( parameters );
                    ctx.Draw( terrain.mIndexCount );
                }

                ctx.EndRender();
            } );

        if ( hdr ) {
            RegisterGrass( graph );
        }
    }

    auto TerrainGenerationModule::UploadGrass( CommandContext &ctx, core::Blackboard &b ) -> void {
        mGrassDraws.clear();
        if ( !mScene || !mCamera ) {
            return;
        }
        const auto &info{ b.Get<TerrainGenerationInfo>() };
        eastl::vector<TerrainGrassDescription> descriptions{};
        eastl::vector<GrassTile> tiles{};
        eastl::vector<DrawIndirectCommand> arguments{};
        eastl::vector<GrassMeshVertex> meshVertices{};
        core::u32 bladeCount{};
        // Each mesh has its own indirect batches but shares the same seeded
        // placement grid. Disjoint weight intervals choose exactly one model
        // per root; adding varieties does not multiply the field's density.
        struct Source {
            core::u32 mTerrain{};
            GrassSettings mSettings{};
            core::float4 mPalette{ 0, 1, 1, 0 };
        };
        eastl::vector<Source> sources{};
        for ( core::u32 terrainIndex{}; terrainIndex < mTerrains.size(); ++terrainIndex ) {
            const auto &grass{ mScene->GetRegistry().get<scene::LandscapeComponent>( mTerrains[terrainIndex].mEntity ).GetSurfaceSettings().mGrass };
            if ( !grass.mEnabled ) {
                continue;
            }
            eastl::vector<GrassMeshVariant> choices{};
            if ( grass.mUseCustomMesh ) {
                choices.push_back( { grass.mMesh, grass.mMeshIndex, grass.mMeshWeight } );
                for ( core::usize i{}; i < glm::min<core::usize>( grass.mMeshVariants.size(), 7 ); ++i ) {
                    choices.push_back( grass.mMeshVariants[i] );
                }
                eastl::erase_if( choices, []( const auto &choice ) {
                    return !choice.mMesh.GetID() || choice.mWeight <= 0;
                } );
            }
            float total{};
            for ( const auto &choice: choices ) {
                total += choice.mWeight;
            }
            float start{};
            if ( choices.empty() ) {
                // Zero-weight entries deliberately disable an authored palette.
                if ( grass.mUseCustomMesh && ( grass.mMesh.GetID() || !grass.mMeshVariants.empty() ) ) {
                    continue;
                }
                auto settings{ grass };
                settings.mUseCustomMesh = false;
                sources.push_back( { terrainIndex, eastl::move( settings ), { 0, 1, 1, 0 } } );
            }
            else {
                for ( const auto &choice: choices ) {
                    auto settings{ grass };
                    settings.mMesh = choice.mMesh;
                    settings.mMeshIndex = choice.mMeshIndex;
                    settings.mHeight *= glm::clamp( choice.mHeightScale, .1f, 4.0f );
                    settings.mWidth *= glm::clamp( choice.mWidthScale, .1f, 4.0f );
                    settings.mMeshVariants.clear();
                    sources.push_back( { terrainIndex, eastl::move( settings ), { start, start + choice.mWeight, total, 0 } } );
                    start += choice.mWeight;
                }
            }
        }
        mGrassMeshes.resize( sources.size() );
        const auto enabledTerrains{ glm::max( 1u, static_cast<core::u32>( sources.size() ) ) };

        for ( core::u32 index{}; index < sources.size(); ++index ) {
            const auto &source{ sources[index] };
            const auto &terrain{ mTerrains[source.mTerrain] };
            const auto &grass{ source.mSettings };
            if ( !grass.mEnabled || bladeCount >= kMaxGrassBlades ) {
                continue;
            }
            const auto world{ mScene->GetRegistry().get<scene::TransformComponent>( terrain.mEntity ).GetWorldTransform() };
            const float scale{ glm::length( core::float3{ world[1] } ) };
            if ( scale < .0001f || glm::abs( glm::determinant( world ) ) < 1e-9f ) {
                continue;
            }
            TerrainGrassDescription description{};
            auto &custom{ mGrassMeshes[index] };
            description.mMesh.z = 0xffffffffu;
            description.mMaterialColor = core::float4{ 1 };
            if ( grass.mUseCustomMesh && grass.mMesh.GetID() ) {
                if ( custom.mRequest.GetRef().GetID() != grass.mMesh.GetID() || custom.mRequest.GetState() == asset::AssetState::eUnloaded ) {
                    custom = {};
                    custom.mRequest = asset::AssetRegistryService::Get()->Request<asset::Model>( grass.mMesh );
                }
                const auto model{ custom.mRequest.GetIfReady() };
                if ( !model || model->IsSkinned() || grass.mMeshIndex >= model->GetMeshNodeCount() ) {
                    continue;
                }
                const auto &mesh{ model->GetMeshNode( grass.mMeshIndex ) };
                const auto &data{ mesh.GetRenderData() };
                if ( !data || data->IsSkinned() || data->GetIndices().size() > 8192 ) {
                    continue;
                }
                if ( custom.mSource != data.GetPtr() || custom.mRevision != data->GetRevision() || custom.mMeshIndex != grass.mMeshIndex ) {
                    custom.mVertices.clear();
                    custom.mTexture = {};
                    custom.mInlineTexture.Reset();
                    custom.mSource = data.GetPtr();
                    custom.mRevision = data->GetRevision();
                    custom.mMeshIndex = grass.mMeshIndex;
                    const auto bytes{ data->GetAttributes() };
                    const auto &offsets{ data->GetOffsets() };
                    const auto stream{ [&]<typename T>( MeshStream kind, core::u32 vertex ) {
                        T result{};
                        std::memcpy( &result, bytes.data() + offsets[static_cast<core::usize>( kind )] + vertex * sizeof( T ), sizeof( T ) );
                        return result;
                    } };
                    const auto transform{ mesh.GetTransform() };
                    const auto normalTransform{ glm::transpose( glm::inverse( core::float3x3{ transform } ) ) };
                    core::float3 minimum{ std::numeric_limits<float>::max() };
                    core::float3 maximum{ -std::numeric_limits<float>::max() };
                    for ( core::u32 vertex{}; vertex < data->GetVertexCount(); ++vertex ) {
                        const auto point{ core::float3{ transform * core::float4{ stream.template operator()<core::float3>( MeshStream::Position, vertex ), 1 } } };
                        minimum = glm::min( minimum, point );
                        maximum = glm::max( maximum, point );
                    }
                    const float height{ maximum.y - minimum.y };
                    const float width{ glm::max( maximum.x - minimum.x, maximum.z - minimum.z ) };
                    if ( height > 1e-5f && width > 1e-5f && data->GetIndices().size() % 3 == 0 ) {
                        const auto center{ ( minimum + maximum ) * .5f };
                        const auto uvStream{ mesh.GetProperties().BaseColorTextureSet == 1 ? MeshStream::UV1 : MeshStream::UV0 };
                        for ( const auto vertex: data->GetIndices() ) {
                            const auto point{ core::float3{ transform * core::float4{ stream.template operator()<core::float3>( MeshStream::Position, vertex ), 1 } } };
                            const auto authoredNormal{ normalTransform * stream.template operator()<core::float3>( MeshStream::Normal, vertex ) };
                            // Inverse transpose of the rest-space normalization. Instance
                            // dimensions are applied independently in the vertex shader.
                            const auto scaledNormal{ authoredNormal * core::float3{ width, height, width } };
                            const auto normal{ glm::dot( scaledNormal, scaledNormal ) > 1e-8f ? glm::normalize( scaledNormal ) : core::float3{ 0, 0, 1 } };
                            const auto uv{ stream.template operator()<core::float2>( uvStream, vertex ) };
                            const core::float3 normalized{ ( point.x - center.x ) / width, ( point.y - minimum.y ) / height, ( point.z - center.z ) / width };
                            custom.mVertices.push_back( { { normalized, uv.y }, { normal, uv.x } } );
                        }
                    }
                    // Model texture references remain asynchronous and resident for
                    // as long as this terrain uses the imported mesh.
                    for ( const auto &[path, binding]: mesh.GetProperties().mTextureBindings ) {
                        if ( binding.mMapType == material::MapType::eBaseColor ) {
                            custom.mInlineTexture = binding.mTexture;
                            if ( !path.IsEmpty() ) {
                                custom.mTexture = asset::AssetRegistryService::Get()->Request<asset::Texture>( asset::AssetRef<asset::Texture>{ path } );
                            }
                            break;
                        }
                    }
                }
                if ( custom.mVertices.empty() ) {
                    continue;
                }
                auto resource{ custom.mInlineTexture };
                if ( custom.mTexture.GetRef().GetID() ) {
                    if ( custom.mTexture.GetState() == asset::AssetState::eUnloaded ) {
                        custom.mTexture = asset::AssetRegistryService::Get()->Request<asset::Texture>( custom.mTexture.GetRef() );
                    }
                    const auto loaded{ custom.mTexture.GetIfReady() };
                    if ( !loaded ) {
                        continue;
                    }
                    resource = loaded->GetResource();
                }
                if ( resource ) {
                    const auto &format{ rhi::GetFormatInfo( resource->GetFormat() ) };
                    description.mMesh.z = ctx.UseImportedTexture( ctx.ImportSampledTexture( resource ) );
                    description.mMesh.w = !format.mIsSRGB && format.mKind != rhi::FormatKind::Float ? 1u : 0u;
                }
                description.mMesh.x = static_cast<core::u32>( meshVertices.size() );
                description.mMesh.y = static_cast<core::u32>( custom.mVertices.size() );
                description.mMaterialColor = mesh.GetProperties().BaseColorFactor;
                const auto alpha{ mesh.GetProperties().mAlphaMask };
                description.mMaterialOptions = { alpha == material::AlphaMode::eBlend ? .5f : mesh.GetProperties().AlphaMaskCutoff,
                    alpha == material::AlphaMode::eOpaque ? 0.0f : 1.0f, resource ? 1.0f : 0.0f, 0 };
                meshVertices.insert( meshVertices.end(), custom.mVertices.begin(), custom.mVertices.end() );
            }
            else {
                custom = {};
            }

            const auto inverse{ glm::inverse( world ) };
            const auto camera{ core::float3{ inverse * core::float4{ mCamera->GetPosition(), 1.0f } } };
            const float distance{ glm::clamp( grass.mViewDistance, 5.0f, kMaxGrassViewDistance ) };
            const float nearDistance{ glm::clamp( grass.mLodDistance, 1.0f, distance ) };
            const float midDistance{ glm::clamp( grass.mFarLodDistance, nearDistance, distance ) };

            // Bound world distance by local XZ distance. The largest singular
            // value of the inverse's planar rows handles rotated/non-uniformly
            // scaled landscapes without discarding potentially visible roots.
            const core::float3 rowX{ inverse[0][0], inverse[1][0], inverse[2][0] };
            const core::float3 rowZ{ inverse[0][2], inverse[1][2], inverse[2][2] };
            const float xx{ glm::dot( rowX, rowX ) };
            const float xz{ glm::dot( rowX, rowZ ) };
            const float zz{ glm::dot( rowZ, rowZ ) };
            const float planarScale{ 1.0f / glm::sqrt( .5f * ( xx + zz + glm::sqrt( ( xx - zz ) * ( xx - zz ) + 4 * xz * xz ) ) ) };
            const float densityDistance{ glm::min( nearDistance, midDistance * .5f ) };
            const float fineSpacing{ glm::max( 1.0f / glm::sqrt( glm::clamp( grass.mDensity, .1f, 100.0f ) ), 2.0f * nearDistance / ( planarScale * 1022.0f ) ) };
            const auto selected{ BuildGrassTiles( GrassTileSelectionDescription{
                .mMinimum = terrain.mWorld.mOrigin,
                .mMaximum = terrain.mWorld.mOrigin + terrain.mWorld.GetWorldSize(),
                .mCamera = { camera.x, camera.z },
                .mSpacing = fineSpacing,
                .mPlanarScale = planarScale,
                .mViewDistance = distance,
                .mDensityDistance = densityDistance,
                .mRootDisplacement = fineSpacing + glm::abs( grass.mClumpStrength ),
            } ) };
            if ( selected.mTiles.empty() ) {
                continue;
            }
            const auto complexity{ glm::max( description.mMesh.y, kGrassHighBladeVertexCount ) };
            const auto capacity{ glm::min( glm::min( 524288u, 16777216u / complexity ), glm::min( kMaxGrassBlades / enabledTerrains / kGrassLodCount, ( kMaxGrassBlades - bladeCount ) / kGrassLodCount ) ) };
            if ( capacity == 0 ) {
                continue;
            }
            const auto tileCount{ static_cast<core::u32>( selected.mTiles.size() ) };
            description.mWorld = world;
            description.mTiling = { fineSpacing, float( tileCount ), 0, 0 };
            // Store offsets until the tile buffer has grown to its final size.
            description.mTiles = tiles.size() * sizeof( GrassTile );
            tiles.insert( tiles.end(), selected.mTiles.begin(), selected.mTiles.end() );
            const float share{ ( source.mPalette.y - source.mPalette.x ) / source.mPalette.z };
            // Both geometry draws share one density budget. Changing blade
            // topology must never change root coverage or compensated width.
            description.mBudgets.x = glm::min( 1.0f, capacity * .9f / glm::max( selected.mCandidateWeight * share, 1.0f ) );
            description.mShape = { grass.mHeight, grass.mWidth, grass.mBend, glm::max( grass.mClumpSize, .2f ) };
            description.mView = { distance, nearDistance, midDistance, float( std::fmod( mScene->GetVolumeTime(), 4096.0 ) ) };
            description.mWind = { grass.mWindDirection, grass.mWindSpeed, grass.mWindStrength };
            description.mRootColor = { grass.mRootColor, grass.mAmbientOcclusion };
            description.mMidColor = { grass.mMidColor, grass.mDryAmount };
            description.mTipColor = { grass.mTipColor, grass.mRoughness };
            description.mDryColor = { grass.mDryColor, grass.mColorVariation };
            description.mLighting = { grass.mTranslucency, grass.mTranslucencyPower, grass.mNormalRounding, grass.mWidthCompensation };
            description.mDetail = { grass.mMinDensity, densityDistance, grass.mWindFrequency, grass.mWindFlutter };
            description.mLimits = { grass.mMinElevation, grass.mMaxElevation, grass.mClumpStrength, glm::cos( glm::radians( grass.mMaxSlope ) ) };
            description.mBudgets.w = grass.mHeightVariation;
            description.mShadows = { grass.mCastShadows ? 1.0f : 0.0f, grass.mShadowDensity,
                grass.mContactShadows ? 1.0f : 0.0f, 0 };
            description.mGrid = { terrain.mWorld.GetResolutionX(), terrain.mWorld.GetResolutionY(), terrain.mFirstVertex, 0 };
            description.mRanges = { bladeCount, capacity, static_cast<core::u32>( descriptions.size() ) * kGrassLodCount, grass.mSeed };
            description.mControl = { glm::min( grass.mLayer, kTerrainSurfaceLayerCount - 1 ), glm::floatBitsToUint( grass.mSelfShadow ),
                grass.mOcclusionEnabled ? 1u : 0u, glm::floatBitsToUint( glm::max( grass.mWindDistance, 1.0f ) ) };
            description.mPalette = source.mPalette;
            description.mSurface = ctx.GetDeviceBufferAddress( info.mSurfaces ) + source.mTerrain * sizeof( TerrainSurfaceData );
            description.mWindField = ctx.GetDeviceBufferAddress( b.Get<WeatherModuleInfo>().mWindField );
            descriptions.push_back( description );
            for ( core::u32 lod{}; lod < kGrassLodCount; ++lod ) {
                const core::u32 vertices{ description.mMesh.y ? description.mMesh.y : ( lod == 0 ? kGrassHighBladeVertexCount : kGrassLowBladeVertexCount ) };
                arguments.push_back( DrawIndirectCommand{ vertices, 0, 0, 0 } );
            }
            mGrassDraws.push_back( GrassDraw{
                .mCandidateCount = tileCount * kGrassTileCandidateCount,
                .mFirstBlade = bladeCount,
                .mCapacity = capacity,
                .mCustomMesh = description.mMesh.y != 0,
                .mCastShadows = grass.mCastShadows && grass.mShadowDensity > 0,
                .mContactShadows = grass.mContactShadows,
                .mProxyVertices = ( terrain.mWorld.GetResolutionX() - 1 ) * ( terrain.mWorld.GetResolutionY() - 1 ) * 6,
            } );
            bladeCount += capacity * kGrassLodCount;
        }
        if ( descriptions.empty() ) {
            return;
        }
        ctx.EnsureBufferCapacity( info.mGrassTiles, tiles.size() * sizeof( GrassTile ) );
        const auto tileAddress{ ctx.GetDeviceBufferAddress( info.mGrassTiles ) };
        for ( auto &description: descriptions ) {
            description.mTiles += tileAddress;
        }
        ctx.Copy( info.mGrassTiles, 0, tiles.data(), tiles.size() * sizeof( GrassTile ) );
        ctx.EnsureBufferCapacity( info.mGrassDescriptions, descriptions.size() * sizeof( TerrainGrassDescription ) );
        ctx.EnsureBufferCapacity( info.mGrassArguments, arguments.size() * sizeof( DrawIndirectCommand ) );
        ctx.EnsureBufferCapacity( info.mGrassBlades, core::usize{ bladeCount } * sizeof( core::float4 ) * 4 );
        ctx.EnsureBufferCapacity( info.mGrassMeshes, glm::max<core::usize>( sizeof( GrassMeshVertex ), meshVertices.size() * sizeof( GrassMeshVertex ) ) );
        ctx.Copy( info.mGrassDescriptions, 0, descriptions.data(), descriptions.size() * sizeof( TerrainGrassDescription ) );
        ctx.Copy( info.mGrassArguments, 0, arguments.data(), arguments.size() * sizeof( DrawIndirectCommand ) );
        if ( !meshVertices.empty() ) {
            ctx.Copy( info.mGrassMeshes, 0, meshVertices.data(), meshVertices.size() * sizeof( GrassMeshVertex ) );
        }
    }

    auto TerrainGenerationModule::RegisterGrassPreparation( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<TerrainGenerationInfo>() };
        // Shadows need paint/placement before lighting exists. The later HDR
        // surface upload supplies lights/IBL without changing this buffer's size.
        graph.RegisterPass(
            "Terrain_UploadShadowSurfaces",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( data.mVertices, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( data.mSurfaces, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( data.mPaintWeights, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                UploadSurfaces( ctx, b, false );
            } );

        info.mGrassDescriptions = graph.Create( FGBufferDescription{}
            .SetName( "Grass_Descriptions" )
            .SetSizeBytes( sizeof( TerrainGrassDescription ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest ) );
        info.mGrassTiles = graph.Create( FGBufferDescription{}
            .SetName( "Grass_Tiles" )
            .SetSizeBytes( sizeof( GrassTile ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest ) );
        info.mGrassBlades = graph.Create( FGBufferDescription{}
            .SetName( "Grass_Blades" )
            .SetSizeBytes( 64 )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest ) );
        info.mGrassArguments = graph.Create( FGBufferDescription{}
            .SetName( "Grass_Arguments" )
            .SetSizeBytes( 32 )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest | rhi::BufferUsageFlagsBits::IndirectDraw ) );
        info.mGrassMeshes = graph.Create( FGBufferDescription{}
            .SetName( "Grass_CustomMeshes" )
            .SetSizeBytes( sizeof( GrassMeshVertex ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest ) );
        const auto [viewportWidth, viewportHeight]{ rhi::InferDimensions( mResolution ) };
        const auto hiZWidth{ ( static_cast<core::u32>( viewportWidth ) + 1 ) / 2 };
        const auto hiZHeight{ ( static_cast<core::u32>( viewportHeight ) + 1 ) / 2 };
        core::u32 hiZLevels{};
        core::usize hiZTexels{};
        for ( core::u32 width{ hiZWidth }, height{ hiZHeight };; ) {
            hiZTexels += core::usize{ width } * height;
            ++hiZLevels;
            if ( width == 1 && height == 1 ) {
                break;
            }
            width = glm::max( ( width + 1 ) / 2, 1u );
            height = glm::max( ( height + 1 ) / 2, 1u );
        }
        info.mGrassHiZ = graph.Create( FGBufferDescription{}
            .SetName( "Grass_HiZ" )
            .SetSizeBytes( hiZTexels * sizeof( float ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured ) );
        info.mGrassDepthPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Grass_DepthPyramid" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "GrassDepth_Comp.slang", FGStageType::eCompute ) );
        info.mGrassGeneratePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Grass_Generate" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "GrassGenerate_Comp.slang", FGStageType::eCompute ) );
        info.mGrassRenderPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Grass_Blades" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleStrip )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Grass_Vert.slang", FGStageType::eVertex )
            .PushShader( "Grass_Frag.slang", FGStageType::ePixel ) );
        info.mGrassMeshPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Grass_CustomMesh" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Grass_Vert.slang", FGStageType::eVertex )
            .PushShader( "Grass_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "Grass_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( b.Get<WeatherModuleInfo>().mWindField, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                // Terrain upload/filtering must finish before recording its surface address.
                builder.UseResource( data.mSurfaces, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( data.mVertices, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                for ( const auto buffer: { data.mGrassDescriptions, data.mGrassTiles, data.mGrassArguments, data.mGrassBlades, data.mGrassMeshes } ) {
                    builder.UseResource( buffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                UploadGrass( ctx, b );
            } );

        // Opaque terrain depth must exist before both Hi-Z and contact rays.
        // Reuse the terrain draw layout, but fetch positions only in this stage.
        info.mTerrainDepthPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Terrain_Depth" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .PushShader( "TerrainDepth_Vert.slang", FGStageType::eVertex )
            .PushShader( "DepthOnly_Frag.slang", FGStageType::ePixel ) );
        graph.RegisterPass(
            "Terrain_Depth",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                builder.UseResource( data.mVertices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( data.mIndices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
            },
            [this, viewportWidth, viewportHeight]( CommandContext &ctx, core::Blackboard &b ) {
                if ( !mScene || mTerrains.empty() ) {
                    return;
                }
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( rhi::Rect{ core::as<core::i32>( viewportWidth ), core::as<core::i32>( viewportHeight ) } )
                        .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, rhi::LoadOp::eLoad ) );
                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect(
                    rhi::Viewport( core::as<float>( viewportWidth ), core::as<float>( viewportHeight ) ) ) );
                ctx.BindPipeline( data.mTerrainDepthPipeline );
                TerrainParameters parameters{
                    .mVertices = ctx.GetDeviceBufferAddress( data.mVertices ),
                    .mIndices = ctx.GetDeviceBufferAddress( data.mIndices ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                };
                for ( const auto &terrain: mTerrains ) {
                    parameters.mWorld = mScene->GetRegistry().get<scene::TransformComponent>( terrain.mEntity ).GetWorldTransform();
                    parameters.mFirstVertex = terrain.mFirstVertex;
                    parameters.mFirstIndex = terrain.mFirstIndex;
                    ctx.PushConstants( parameters );
                    ctx.Draw( terrain.mIndexCount );
                }
                ctx.EndRender();
            } );


        // One reduction per graph pass makes every read-after-write explicit.
        // Current-frame opaque depth avoids stale occluders when the camera moves.
        core::u32 previousWidth{ static_cast<core::u32>( viewportWidth ) };
        core::u32 previousHeight{ static_cast<core::u32>( viewportHeight ) };
        core::u32 previousOffset{};
        core::u32 outputOffset{};
        for ( core::u32 level{}; level < hiZLevels; ++level ) {
            const core::u32 width{ glm::max( ( previousWidth + 1 ) / 2, 1u ) };
            const core::u32 height{ glm::max( ( previousHeight + 1 ) / 2, 1u ) };
            graph.RegisterPass(
                string::Format( "Grass_HiZ_{}", level ),
                FGPassType::eCompute,
                []( FGNodeBuilder &builder, core::Blackboard &b ) {
                    builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<TerrainGenerationInfo>().mGrassHiZ, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                    // UploadGrass publishes the CPU draw list as well as these
                    // descriptions; force it before a pyramid pass can early-out.
                    builder.UseResource( b.Get<TerrainGenerationInfo>().mGrassDescriptions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                },
                [this, level, width, height, previousWidth, previousHeight, previousOffset, outputOffset]( CommandContext &ctx, core::Blackboard &b ) {
                    if ( mGrassDraws.empty() ) {
                        return;
                    }
                    const auto &data{ b.Get<TerrainGenerationInfo>() };
                    struct Parameters {
                        core::u32 mDepth{};
                        core::u32 mOutput{};
                        core::u32 mWidth{};
                        core::u32 mHeight{};
                        core::u32 mSourceWidth{};
                        core::u32 mSourceHeight{};
                        core::u32 mSourceOffset{};
                        core::u32 mOutputOffset{};
                        core::u32 mFirstLevel{};
                    } parameters{
                        .mDepth = ctx.PushSRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget ),
                        .mOutput = ctx.PushUAV( data.mGrassHiZ ),
                        .mWidth = width,
                        .mHeight = height,
                        .mSourceWidth = previousWidth,
                        .mSourceHeight = previousHeight,
                        .mSourceOffset = previousOffset,
                        .mOutputOffset = outputOffset,
                        .mFirstLevel = level == 0 ? 1u : 0u,
                    };
                    ctx.BindPipeline( data.mGrassDepthPipeline );
                    ctx.PushConstants( parameters );
                    ctx.Dispatch( ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
                } );
            previousWidth = width;
            previousHeight = height;
            previousOffset = outputOffset;
            outputOffset += width * height;
        }

        const auto declareCompute{ []( FGNodeBuilder &builder, core::Blackboard &b ) {
            const auto &data{ b.Get<TerrainGenerationInfo>() };
            builder.UseResource( b.Get<WeatherModuleInfo>().mWindField, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            for ( const auto buffer: { data.mGrassDescriptions, data.mGrassTiles, data.mVertices, data.mSurfaces, data.mPaintWeights, data.mGrassHiZ,
                      b.Get<CameraModuleInfo>().mCameraData } ) {
                builder.UseResource( buffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            }
            builder.UseResource( data.mGrassArguments, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            builder.UseResource( data.mGrassBlades, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
        } };
        const auto dispatch{ [this, viewportWidth, viewportHeight, hiZLevels]( CommandContext &ctx, core::Blackboard &b, bool finalize ) {
            if ( mGrassDraws.empty() ) {
                return;
            }
            const auto &data{ b.Get<TerrainGenerationInfo>() };
            TerrainGrassParameters parameters{};
            parameters.mDescriptions = ctx.GetDeviceBufferAddress( data.mGrassDescriptions );
            parameters.mVertices = ctx.GetDeviceBufferAddress( data.mVertices );
            parameters.mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData );
            parameters.mBlades = ctx.GetDeviceBufferAddress( data.mGrassBlades );
            parameters.mHiZ = ctx.GetDeviceBufferAddress( data.mGrassHiZ );
            parameters.mArguments = ctx.PushUAV( data.mGrassArguments );
            parameters.mFinalize = finalize ? 1u : 0u;
            parameters.mHiZWidth = static_cast<core::u32>( viewportWidth );
            parameters.mHiZHeight = static_cast<core::u32>( viewportHeight );
            parameters.mHiZLevels = hiZLevels;
            ctx.BindPipeline( data.mGrassGeneratePipeline );
            for ( core::u32 index{}; index < mGrassDraws.size(); ++index ) {
                parameters.mTerrain = index;
                // Two dimensions keep large visible tile sets below the API's
                // guaranteed per-axis dispatch limit without dropping candidates.
                constexpr core::u32 kMaxGroupsPerAxis{ 65535 };
                const auto candidates{ mGrassDraws[index].mCandidateCount };
                const auto groups{ finalize ? 1u : ( candidates + kGrassGenerateGroupSize - 1 ) / kGrassGenerateGroupSize };
                const auto groupsX{ glm::min( groups, kMaxGroupsPerAxis ) };
                parameters.mDispatchWidth = groupsX * kGrassGenerateGroupSize;
                ctx.PushConstants( parameters );
                ctx.Dispatch( groupsX, ( groups + groupsX - 1 ) / groupsX, 1 );
            }
        } };
        graph.RegisterPass(
            "Grass_Generate",
            FGPassType::eCompute,
            declareCompute,
            [dispatch]( CommandContext &ctx, core::Blackboard &b ) {
                dispatch( ctx, b, false );
            } );
        // A separate dispatch provides a graph-managed UAV barrier before clamping counters.
        graph.RegisterPass(
            "Grass_Finalize",
            FGPassType::eCompute,
            declareCompute,
            [dispatch]( CommandContext &ctx, core::Blackboard &b ) {
                dispatch( ctx, b, true );
            } );

        // Keep the opaque receiver depth intact. Grass writes a second depth
        // image plus a coverage mask, so contact rays can identify real blades.
        info.mGrassContactDepth = graph.Create( FGTextureDescription{}
            .SetName( "Grass_ContactDepth" )
            .SetWidth( viewportWidth )
            .SetHeight( viewportHeight )
            .SetFormat( rhi::Format::eD32 )
            .SetUsage( rhi::TextureUsageFlagsBits::DepthTarget | rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::CopyDest ) );
        info.mGrassContactMask = graph.Create( FGTextureDescription{}
            .SetName( "Grass_ContactCoverage" )
            .SetWidth( viewportWidth )
            .SetHeight( viewportHeight )
            .SetFormat( rhi::Format::eR8_UNORM )
            .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource ) );
        const auto depthPipeline{ [&]( const char *name, rhi::PrimitiveTopology topology ) {
            return graph.Create( FGPipelineDescription{}
                .SetName( name )
                .SetPipelineType( rhi::PipelineType::eGraphics )
                .SetTopology( topology )
                .SetCullMode( rhi::CullMode::eNone )
                .SetDepthTest( true )
                .SetDepthWrite( true )
                .SetDepthFormat( rhi::Format::eD32 )
                .AddColorFormat( rhi::Format::eR8_UNORM )
                .PushShader( "Grass_Vert.slang", FGStageType::eVertex )
                .PushShader( "GrassDepth_Frag.slang", FGStageType::ePixel ) );
        } };
        info.mGrassContactDepthPipeline = depthPipeline( "Grass_ContactBlades", rhi::PrimitiveTopology::eTriangleStrip );
        info.mGrassContactMeshDepthPipeline = depthPipeline( "Grass_ContactMeshes", rhi::PrimitiveTopology::eTriangleList );
        graph.RegisterPass(
            "Grass_CopyReceiverDepth",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( b.Get<TerrainGenerationInfo>().mGrassContactDepth, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [viewportWidth, viewportHeight]( CommandContext &ctx, core::Blackboard &b ) {
                const rhi::TextureSlice slice{
                    .mWidth = core::as<core::u32>( viewportWidth ),
                    .mHeight = core::as<core::u32>( viewportHeight ),
                };
                ctx.Copy( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, slice,
                    b.Get<TerrainGenerationInfo>().mGrassContactDepth, slice );
            } );
        graph.RegisterPass(
            "Grass_ContactDepth",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                for ( const auto buffer: { data.mGrassDescriptions, data.mGrassBlades, data.mGrassMeshes, b.Get<CameraModuleInfo>().mCameraData } ) {
                    builder.UseResource( buffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                }
                builder.UseResource( data.mGrassDescriptions, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( data.mSurfaces, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( data.mGrassArguments, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
                builder.UseResource( data.mGrassContactDepth, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( data.mGrassContactMask, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, viewportWidth, viewportHeight]( CommandContext &ctx, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( rhi::Rect{ core::as<core::i32>( viewportWidth ), core::as<core::i32>( viewportHeight ) } )
                        .AddRenderTarget( data.mGrassContactMask, rhi::Color{}, rhi::LoadOp::eClear )
                        .AddDepthTarget( data.mGrassContactDepth, rhi::LoadOp::eLoad ) );
                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect(
                    rhi::Viewport( core::as<float>( viewportWidth ), core::as<float>( viewportHeight ) ) ) );
                TerrainGrassParameters parameters{
                    .mDescriptions = ctx.GetDeviceBufferAddress( data.mGrassDescriptions ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                    .mBlades = ctx.GetDeviceBufferAddress( data.mGrassBlades ),
                    .mMeshes = ctx.GetDeviceBufferAddress( data.mGrassMeshes ),
                };
                for ( core::u32 index{}; index < mGrassDraws.size(); ++index ) {
                    if ( !mGrassDraws[index].mCastShadows || !mGrassDraws[index].mContactShadows ) {
                        continue;
                    }
                    parameters.mTerrain = index;
                    ctx.BindPipeline( mGrassDraws[index].mCustomMesh ? data.mGrassContactMeshDepthPipeline : data.mGrassContactDepthPipeline );
                    for ( core::u32 lod{}; lod < kGrassLodCount; ++lod ) {
                        parameters.mLod = lod;
                        ctx.PushConstants( parameters );
                        ctx.DrawIndirect( DrawIndirectState{}
                                .SetBuffer( data.mGrassArguments )
                                .SetFirstDraw( index * kGrassLodCount + lod )
                                .SetDrawCount( 1 ) );
                    }
                }
                ctx.EndRender();
            } );
        auto &shadows{ graph.GetOrCreate<ShadowMapInfo>() };
        for ( core::u32 light{}; light < kMaxShadowLightsPerType; ++light ) {
            // R/A retain full precision depth for opaque/grass receiver matching;
            // G/B carry their separate contact visibilities. No shadow ABI change.
            shadows.mGrassReceiverData[light] = graph.Create( FGTextureDescription{}
                .SetName( string::Format( "Grass_ReceiverData_{}", light ) )
                .SetWidth( viewportWidth )
                .SetHeight( viewportHeight )
                .SetFormat( rhi::Format::eRGBA32_FLOAT )
                .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::UnorderedAccess ) );
        }
    }

    auto TerrainGenerationModule::RegisterGrassShadows( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<TerrainGenerationInfo>() };
        auto &shadows{ graph.GetOrCreate<ShadowMapInfo>() };
        info.mGrassShadowPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Grass_CanopyShadow" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .PushShader( "GrassShadow_Vert.slang", FGStageType::eVertex )
            .PushShader( "GrassShadow_Frag.slang", FGStageType::ePixel ) );
        graph.RegisterPass(
            "Grass_CanopyShadows",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                const auto &shadow{ b.Get<ShadowMapInfo>() };
                for ( const auto buffer: { data.mGrassDescriptions, data.mVertices, shadow.mRecordsBuffer, b.Get<CameraModuleInfo>().mCameraData } ) {
                    builder.UseResource( buffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                }
                for ( const auto buffer: { data.mGrassDescriptions, data.mSurfaces, data.mPaintWeights, shadow.mRecordsBuffer, b.Get<CameraModuleInfo>().mCameraData } ) {
                    builder.UseResource( buffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                for ( const auto &light: shadow.mDirectional ) {
                    for ( core::u32 face{}; face < shadow.mDirectionalCascadeCount; ++face ) {
                        builder.UseResource( light[face], FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                    }
                }
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                if ( mGrassDraws.empty() ) {
                    return;
                }
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                const auto &shadow{ b.Get<ShadowMapInfo>() };
                struct Parameters {
                    SPointer mDescriptions{};
                    SPointer mVertices{};
                    SPointer mRecords{};
                    SPointer mCamera{};
                    core::u32 mDescription{};
                    core::u32 mRecord{};
                    core::u32 mFace{};
                    core::u32 mPadding{};
                    core::float3 mToLight{};
                    core::u32 mPadding1{};
                } parameters{
                    .mDescriptions = ctx.GetDeviceBufferAddress( data.mGrassDescriptions ),
                    .mVertices = ctx.GetDeviceBufferAddress( data.mVertices ),
                    .mRecords = ctx.GetDeviceBufferAddress( shadow.mRecordsBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                };
                static_assert( sizeof( Parameters ) == 64 );
                for ( core::u32 light{}; light < kMaxShadowLightsPerType; ++light ) {
                    const auto record{ shadow.mDirectionalRecords[light] };
                    if ( record >= shadow.mRecordCount ) {
                        continue;
                    }
                    parameters.mRecord = record;
                    parameters.mToLight = shadow.mDirectionalToLight[light];
                    const auto size{ shadow.mDirectionalSize };
                    for ( core::u32 face{}; face < shadow.mDirectionalCascadeCount; ++face ) {
                        parameters.mFace = face;
                        ctx.BeginRender( RenderBeginDescription{}
                                .SetRenderArea( rhi::Rect{ core::as<core::i32>( size ), core::as<core::i32>( size ) } )
                                .AddDepthTarget( shadow.mDirectional[light][face], rhi::LoadOp::eLoad ) );
                        ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect(
                            rhi::Viewport( core::as<float>( size ), core::as<float>( size ) ) ) );
                        ctx.BindPipeline( data.mGrassShadowPipeline );
                        for ( core::u32 index{}; index < mGrassDraws.size(); ++index ) {
                            if ( !mGrassDraws[index].mCastShadows ) {
                                continue;
                            }
                            parameters.mDescription = index;
                            ctx.PushConstants( parameters );
                            ctx.Draw( mGrassDraws[index].mProxyVertices );
                        }
                        ctx.EndRender();
                    }
                }
            } );

        info.mGrassContactPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Grass_ContactShadows" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "GrassContact_Comp.slang", FGStageType::eCompute ) );
        for ( core::u32 light{}; light < kMaxShadowLightsPerType; ++light ) {
            // Receiver data has its own dependencies; the bounded mMaps arena
            // continues to contain depth maps only.
            graph.RegisterPass(
                string::Format( "Grass_ContactShadows_{}", light ),
                FGPassType::eCompute,
                [light]( FGNodeBuilder &builder, core::Blackboard &b ) {
                    const auto &data{ b.Get<TerrainGenerationInfo>() };
                    builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( data.mGrassContactDepth, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( data.mGrassContactMask, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( data.mGrassDescriptions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<ShadowMapInfo>().mRecordsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<ShadowMapInfo>().mGrassReceiverData[light], FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                },
                [this, light]( CommandContext &ctx, core::Blackboard &b ) {
                    const auto &data{ b.Get<TerrainGenerationInfo>() };
                    const auto &shadow{ b.Get<ShadowMapInfo>() };
                    if ( shadow.mDirectionalRecords[light] >= shadow.mRecordCount ) {
                        return;
                    }
                    const bool active{ eastl::any_of( mGrassDraws.begin(), mGrassDraws.end(), []( const GrassDraw &draw ) {
                        return draw.mCastShadows && draw.mContactShadows;
                    } ) };
                    struct Parameters {
                        SPointer mCamera{};
                        core::u32 mSceneDepth{};
                        core::u32 mGrassDepth{};
                        core::u32 mCoverage{};
                        core::u32 mOutput{};
                        core::u32 mActive{};
                        core::float3 mToLight{};
                    } parameters{
                        .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                        .mSceneDepth = ctx.PushSRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget ),
                        .mGrassDepth = ctx.PushSRV( data.mGrassContactDepth ),
                        .mCoverage = ctx.PushSRV( data.mGrassContactMask ),
                        .mOutput = ctx.PushUAV( shadow.mGrassReceiverData[light] ),
                        .mActive = active ? 1u : 0u,
                        .mToLight = shadow.mDirectionalToLight[light],
                    };
                    static_assert( sizeof( Parameters ) == 40 );
                    const auto [width, height]{ rhi::InferDimensions( mResolution ) };
                    ctx.BindPipeline( data.mGrassContactPipeline );
                    ctx.PushConstants( parameters );
                    ctx.Dispatch( ( core::as<core::u32>( width ) + 7 ) / 8, ( core::as<core::u32>( height ) + 7 ) / 8, 1 );
                } );
        }
    }


    auto TerrainGenerationModule::RegisterGrass( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "Grass_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                for ( const auto buffer: { data.mGrassDescriptions, data.mGrassBlades, data.mGrassMeshes, b.Get<CameraModuleInfo>().mCameraData } ) {
                    builder.UseResource( buffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                }
                builder.UseResource( data.mGrassDescriptions, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( data.mSurfaces, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( data.mGrassArguments, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mLightsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                const auto &lighting{ b.Get<GeomShadingModuleInfo>() };
                builder.UseResource( lighting.mIrradianceCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                const auto &shadows{ b.Get<ShadowMapInfo>() };
                builder.UseResource( shadows.mRecordsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                for ( const auto map: shadows.mMaps ) {
                    builder.UseResource( map, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                for ( const auto receiver: shadows.mGrassReceiverData ) {
                    if ( receiver.mHandle ) {
                        builder.UseResource( receiver, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                }
                if ( const auto *clouds{ b.TryGet<CloudsModuleInfo>() } ) {
                    builder.UseResource( clouds->mFrame, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds->mModels, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    for ( const auto &volume: clouds->mVolumes ) {
                        builder.UseResource( volume.mLighting, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                }
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( lighting.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                if ( mGrassDraws.empty() ) {
                    return;
                }
                const auto &data{ b.Get<TerrainGenerationInfo>() };
                const auto [width, height]{ rhi::InferDimensions( mResolution ) };
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
                    .AddRenderTarget( b.Get<GeomShadingModuleInfo>().mColorImage, rhi::Color{}, rhi::LoadOp::eLoad )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, rhi::LoadOp::eLoad ) );
                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect( rhi::Viewport( core::as<float>( width ), core::as<float>( height ) ) ) );
                TerrainGrassParameters parameters{};
                parameters.mDescriptions = ctx.GetDeviceBufferAddress( data.mGrassDescriptions );
                parameters.mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData );
                parameters.mBlades = ctx.GetDeviceBufferAddress( data.mGrassBlades );
                parameters.mMeshes = ctx.GetDeviceBufferAddress( data.mGrassMeshes );
                for ( core::u32 index{}; index < mGrassDraws.size(); ++index ) {
                    parameters.mTerrain = index;
                    ctx.BindPipeline( mGrassDraws[index].mCustomMesh ? data.mGrassMeshPipeline : data.mGrassRenderPipeline );
                    for ( core::u32 lod{}; lod < kGrassLodCount; ++lod ) {
                        parameters.mLod = lod;
                        ctx.PushConstants( parameters );
                        ctx.DrawIndirect( DrawIndirectState{}.SetBuffer( data.mGrassArguments ).SetFirstDraw( index * kGrassLodCount + lod ).SetDrawCount( 1 ) );
                    }
                }
                ctx.EndRender();
            } );
    }

}// namespace mikoto::renderer
