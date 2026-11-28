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
#include <exception>
#include <limits>

#include <ankerl/unordered_dense.h>

#include <EASTL/array.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <Animation/AnimationSystem.hh>
#include <Animation/SkinningPalette.hh>
#include <Animation/Utility.hh>

#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Material/Bssrdf.hh>
#include <Material/SurfaceMaterial.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>
#include <Renderer/RayTracing/RayTracing.hh>
#include <Renderer/Video/Video.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Scene/Wind.hh>

namespace mikoto::renderer {
    using namespace core;
    using namespace rhi;
    using namespace scene;

    static auto CreateUploadBuffer( FrameGraph &graph, cstr name, usize size, BufferUsageFlags extraUsage = {} ) -> FGBufferHandle {
        return graph.Create( FGBufferDescription{}
            .SetName( name )
            .SetSizeBytes( size )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest | extraUsage ) );
    }

    static auto AppendSceneBytes( eastl::vector<ubyte> &data, const void *source, usize size ) -> void {
        if ( size == 0 ) {
            return;
        }

        const auto offset{ data.size() };
        data.resize( offset + size );
        std::memcpy( data.data() + offset, source, size );
    }

    // Repeat + bilinear sampling matches the material sampler's level-zero height.
    // Displacement changes traversal geometry, not just the shading normal.
    // https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-18-using-vertex-texture-displacement
    static auto SampleDisplacementHeight( const asset::Image &image, float2 uv ) -> f32 {
        const auto size{ glm::ivec2{ image.mWidth, image.mHeight } };
        const auto pixel{ uv * float2{ size } - 0.5f };
        const auto origin{ glm::ivec2{ glm::floor( pixel ) } };
        const auto blend{ glm::fract( pixel ) };
        const auto sample{ [&]( i32 x, i32 y ) {
            const auto wrappedX{ ( x % size.x + size.x ) % size.x };
            const auto wrappedY{ ( y % size.y + size.y ) % size.y };
            const auto offset{ ( as<usize>( wrappedY ) * image.mWidth + wrappedX ) * image.mChannels };
            if ( image.mFormat == asset::ImageFormat::eRGBA32_FLOAT ) {
                return static_cast<const f32 *>( image.mBufferSpan->GetData() )[offset];
            }
            return static_cast<const ubyte *>( image.mBufferSpan->GetData() )[offset] / 255.0f;
        } };
        return glm::mix( glm::mix( sample( origin.x, origin.y ), sample( origin.x + 1, origin.y ), blend.x ),
            glm::mix( sample( origin.x, origin.y + 1 ), sample( origin.x + 1, origin.y + 1 ), blend.x ), blend.y );
    }

    // Match base/Foliage_Helpers.slang in world space. The stored vertices are
    // the actual current pose; the RR snapshot retains the preceding rendered
    // pose, covering wind deformation together with camera and instance movement.
    MKT_NODISCARD static auto DeformFoliagePosition( const float3 &position, const MeshInstance &mesh ) -> float3 {
        if ( mesh.mFoliageMotion.x <= 0.0f ) {
            return position;
        }

        constexpr f32 referenceWindSpeed{ 12.0f };
        // Anchor the plant at its authored local root and keep that base fixed.
        const auto root{ float3{ mesh.mTransform * float4{ 0, mesh.mFoliageShape.x, 0, 1 } } };
        const auto axis{ float3{ mesh.mTransform * float4{ 0, 1, 0, 0 } } };
        const auto scale{ glm::max( glm::length( axis ), 1e-6f ) };
        const auto up{ axis / scale };
        const auto delta{ position - root };
        const auto elevation{ glm::dot( delta, up ) };
        if ( elevation <= 0.0f ) {
            return position;
        }

        const auto velocity{ float3{ mesh.mFoliageWind } };
        // Only wind perpendicular to the trunk bends the surface.
        const auto wind{ velocity - up * glm::dot( velocity, up ) };
        const auto speed{ glm::length( wind ) };
        if ( speed < 1e-5f ) {
            return position;
        }

        const auto direction{ wind / speed };
        const auto side{ glm::cross( up, direction ) };
        const auto height{ mesh.mFoliageMotion.x * scale };
        const auto weight{ glm::clamp( elevation / height, 0.0f, 1.0f ) };
        const auto strength{ glm::clamp( speed / referenceWindSpeed, 0.0f, 1.0f ) };
        const auto time{ mesh.mFoliageWind.w * mesh.mFoliageShape.y };
        const auto phase{ glm::dot( root, float3{ .173f, .137f, .291f } ) };
        // Slow trunk sway carries the branches; radial phase and faster waves
        // add branch motion and leaf flutter without moving the root.
        const auto mainWave{ 1.0f + .2f * std::sin( time * .73f + phase ) + .1f * std::sin( time * 1.31f + phase * 1.7f ) };
        const auto bend{ height * mesh.mFoliageMotion.y * weight * weight * strength };
        const auto radial{ delta - up * elevation };
        const auto branchWeight{ weight * weight * glm::clamp( glm::length( radial ) / glm::max( height * .2f, .05f ), 0.0f, 1.0f ) };
        const auto branchPhase{ glm::dot( radial, direction * .7f + side * .5f ) + phase };
        const auto branch{ std::sin( time * 2.17f + branchPhase ) * mesh.mFoliageMotion.z * scale * branchWeight * strength };
        const auto flutter{ ( std::sin( time * 7.3f + branchPhase * 2.1f ) + .35f * std::sin( time * 11.7f + branchPhase * 3.7f ) ) *
                            mesh.mFoliageMotion.w * scale * weight * strength };
        return position + direction * ( bend * mainWave + branch ) +
               side * ( bend * .15f * std::sin( time * .91f + phase ) + flutter ) -
               up * ( bend * bend / glm::max( 2.0f * elevation, .01f ) );
    }

    MKT_NODISCARD static auto DeformFoliageNormal( const float3 &rest, const float3 &deformed,
        const float3 &normal, const MeshInstance &mesh ) -> float3 {
        constexpr f32 normalSampleDistance{ .01f };
        const auto reference{ std::abs( normal.y ) < .9f ? float3{ 0, 1, 0 } : float3{ 1, 0, 0 } };
        const auto tangent{ glm::normalize( glm::cross( reference, normal ) ) };
        const auto bitangent{ glm::cross( normal, tangent ) };
        const auto a{ DeformFoliagePosition( rest + tangent * normalSampleDistance, mesh ) - deformed };
        const auto b{ DeformFoliagePosition( rest + bitangent * normalSampleDistance, mesh ) - deformed };
        const auto result{ glm::cross( a, b ) };
        return glm::dot( result, result ) > 1e-12f ? glm::normalize( result ) : normal;
    }

    // PBRT attaches radiance to the shape, independently of its scattering material.
    // A model-root light covers its descendant meshes; a nearer light overrides that binding.
    // https://pbr-book.org/4ed/Light_Sources/Area_Lights
    static auto FindMeshAreaLight( const Scene &scene, entt::entity entity ) -> const AreaLight * {
        const auto &registry{ scene.GetRegistry() };

        while ( entity != entt::null ) {
            if ( const auto *component{ registry.try_get<LightComponent>( entity ) };
                component && component->IsTypeActive( LightType::eAreaLight ) ) {
                const auto &light{ component->Get<AreaLight>() };
                const auto *tag{ registry.try_get<TagComponent>( entity ) };
                return tag && tag->IsActive() && light.GetShape() == AreaLightShape::eMesh ? &light : nullptr;
            }

            const auto *relation{ registry.try_get<RelationComponent>( entity ) };
            const auto *parent{ relation && relation->HasParent() ? scene.FindByID( *relation->GetParent() ) : nullptr };
            entity = parent ? parent->Get() : entt::null;
        }

        return nullptr;
    }

    // PBRT DiffuseAreaLight: constant emitted radiance on a finite shape.
    // Use a local XY rectangle facing +Z; two triangles share the existing emitter sampler.
    // https://pbr-book.org/4ed/Light_Sources/Area_Lights
    // https://pbr-book.org/4ed/Shapes/Triangle_Meshes
    static auto BuildRectangleEmitter( const AreaLight &light, const float4x4 &world,
        RectangleEmitter &shape ) -> bool {
        const auto halfSize{ 0.5f * light.GetSize() };
        const float3 radiance{ light.GetColor() * light.GetIntensity() };

        if ( halfSize.x <= 0.0f || halfSize.y <= 0.0f ) {
            return false;
        }

        const eastl::array<float2, 4> corners{
            float2{ -halfSize.x, -halfSize.y }, float2{ halfSize.x, -halfSize.y },
            float2{ halfSize.x, halfSize.y }, float2{ -halfSize.x, halfSize.y }
        };

        for ( usize index{}; index < corners.size(); ++index ) {
            shape.mCorners[index] = world * float4{ corners[index], 0, 1 };
        }

        // Keep the local +Z emission side under reflections, just as PBRT transforms normals.
        if ( glm::determinant( glm::mat3{ world } ) < 0.0f ) {
            eastl::swap( shape.mCorners[1], shape.mCorners[3] );
        }

        const auto normal{ glm::cross( shape.mCorners[1] - shape.mCorners[0], shape.mCorners[3] - shape.mCorners[0] ) };
        const f32 area{ glm::length( normal ) };

        if ( area <= 0.0f ) {
            return false;
        }

        shape.mNormal = normal / area;
        shape.mRadiance = glm::max( radiance, float3{ 0 } );
        shape.mTwoSided = light.IsTwoSided();
        return true;
    }

    static auto AbsoluteLinear( const float4x4 &transform ) -> glm::mat3 {
        return { glm::abs( float3{ transform[0] } ), glm::abs( float3{ transform[1] } ), glm::abs( float3{ transform[2] } ) };
    }

    // Sampled emitters need the same transform/traversal uncertainty as hardware hits.
    // A mesh extent bounds every triangle; the scalar bound fits the unused normal.w.
    // https://pbr-book.org/4ed/Shapes/Managing_Rounding_Error
    // https://developer.nvidia.com/blog/solving-self-intersection-artifacts-in-directx-raytracing/
    static auto SampledPositionError( const float3 &local, const float3 &world, f32 extent,
        const glm::mat3 &linear, const float3 &translation,
        const glm::mat3 &inverseLinear, const float3 &inverseTranslation ) -> f32 {
        constexpr f32 c0{ 5.9604644775390625e-8f };
        constexpr f32 c1{ 1.7881397695873602e-7f };
        constexpr f32 c2{ 1.1920931797249068e-7f };
        const float3 objectError{ c0 * glm::abs( local ) + c1 * extent +
                                  c2 * ( inverseLinear * glm::abs( world ) + inverseTranslation ) };
        const float3 worldError{ linear * objectError +
                                 ( c1 + c2 ) * ( linear * glm::abs( local ) ) + 2.0f * c2 * translation };
        return glm::max( worldError.x, glm::max( worldError.y, worldError.z ) ) * 1.000001f;
    }

    auto RayTracingModule::HashBytes( core::u64 &hash, const void *data, core::usize bytes ) -> void {
        const auto *values{ static_cast<const core::ubyte *>( data ) };

        for ( core::usize i{}; i < bytes; ++i ) {
            hash = ( hash ^ values[i] ) * kRevisionHashPrime;
        }
    }

    template<typename T>
    auto RayTracingModule::Hash( core::u64 &hash, const T &value ) -> void {
        HashBytes( hash, &value, sizeof( value ) );
    }

    RayTracingModule::RayTracingModule( rhi::RenderResolution resolution )
        : RayTracingModule{ rhi::GetRenderDimensions( resolution ) } {
    }

    RayTracingModule::RayTracingModule( core::uint2 dimensions )
        : mDimensions{ dimensions } {
    }

    auto RayTracingModule::RegisterUploadPass( FrameGraph &graph ) -> void {
        RegisterGeometryPreparePass( graph );
        RegisterGeometryTransferPass( graph );
    }

    auto RayTracingModule::RegisterGeometryPreparePass( FrameGraph &graph ) -> void {
        // Allocate an initial buffer for each stream. The upload grows them to fit the scene.
        mShadingVertexBuffer = CreateUploadBuffer( graph, "RayTracing_Vertices", sizeof( Vertex ) );
        mAccelerationBuildPositionBuffer = CreateUploadBuffer( graph, "RayTracing_BuildPositions", kTriangleIndexCount * kPositionStreamStride,
            BufferUsageFlagsBits::AccelStructureBuildInput );
        mIndexBuffer = CreateUploadBuffer( graph, "RayTracing_Indices", kTriangleIndexCount * sizeof( u32 ),
            BufferUsageFlagsBits::AccelStructureBuildInput );

        graph.RegisterPass(
            "RayTracing_PrepareGeometry",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                for ( const auto handle: { mShadingVertexBuffer, mAccelerationBuildPositionBuffer, mIndexBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                mGeometryUploads.Clear();

                if ( !mCanTraceFrame || !mGeometryUploadPending ) {
                    return;
                }

                try {
                    PrepareGeometryUpload( context );
                } catch ( const std::exception &error ) {
                    RecordTraceError( error.what() );
                }
            } );
    }

    auto RayTracingModule::RegisterGeometryTransferPass( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "RayTracing_UploadGeometry",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                for ( const auto handle: { mShadingVertexBuffer, mAccelerationBuildPositionBuffer, mIndexBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                if ( !mCanTraceFrame || !mGeometryUploadPending ) {
                    return;
                }

                mGeometryUploads.Submit( context );
                mAccelerationBuildPending = true;
            } );
    }

    auto RayTracingModule::RegisterBuildPass( FrameGraph &graph ) -> void {
        mSceneAccelerationStructure = graph.Create( FGAccelerationStructureDescription{ "RayTracing_AccelerationStructure" } );

        graph.RegisterPass(
            "RayTracing_BuildAccelerationStructures",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mAccelerationBuildPositionBuffer, FGPipelineStage::eAccelStructBuildInput, FGResourceAccess::eRead );
                builder.UseResource( mIndexBuffer, FGPipelineStage::eAccelStructBuildInput, FGResourceAccess::eRead );
                builder.UseResource( mSceneAccelerationStructure, FGPipelineStage::eAccelStructWrite, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                if ( !mCanTraceFrame || !mAccelerationBuildPending ) {
                    return;
                }

                // The command context reuses unchanged mesh BLAS entries and builds the TLAS.
                // https://docs.vulkan.org/guide/latest/extensions/ray_tracing.html#_vk_khr_acceleration_structure
                try {
                    context.BuildAccelerationStructures( mSceneAccelerationStructure, mAccelerationBuildPositionBuffer,
                        mIndexBuffer, mPackedScene.mMeshes );
                    mAccelerationBuildPending = false;
                    mGeometryUploadPending = false;
                } catch ( const std::exception &error ) {
                    RecordTraceError( error.what() );
                }
            } );
    }

    auto RayTracingModule::RegisterFramePass( FrameGraph &graph ) -> void {
        RegisterFramePreparePass( graph );
        RegisterFrameTransferPass( graph );
    }

    auto RayTracingModule::RegisterFramePreparePass( FrameGraph &graph ) -> void {
        mSceneDataBuffer = CreateUploadBuffer( graph, "RayTracing_MaterialsAndLights", sizeof( SceneHeader ) );
        mViewEffectsBuffer = CreateUploadBuffer( graph, "RayTracing_ViewEffects", sizeof( ViewEffects ) );
        mFrameConstantsBuffer = CreateUploadBuffer( graph, "RayTracing_Frame", sizeof( FrameConstants ) );

#if MIKOTO_ENABLE_NRD
        if ( mDenoiserSettings.mEnabled ) {
            mDenoiserDataBuffer = CreateUploadBuffer( graph, "Denoiser_RayData", sizeof( DenoiserRayData ) );
            mPreviousPositionsBuffer = CreateUploadBuffer( graph, "Denoiser_PreviousPositions", sizeof( core::float4 ) );
        }
#endif
        if ( mDenoiserSettings.mReconstructionGuides ) {
            mReconstructionDataBuffer = CreateUploadBuffer( graph, "DLSS_RayData", sizeof( DlssRayData ) );
            mReconstructionPositionsBuffer = CreateUploadBuffer( graph, "DLSS_PreviousPositions", sizeof( core::float4 ) );
        }
        graph.RegisterPass(
            "RayTracing_PrepareFrame",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, core::Blackboard &b ) {
                if ( const auto *volumes{ b.TryGet<VolumeModuleInfo>() } ) {
                    builder.UseResource( volumes->mData, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                }

                // Frame preparation requests the TLAS descriptor after the build pass runs.
                builder.UseResource( mSceneAccelerationStructure, FGPipelineStage::eAccelStructRead, FGResourceAccess::eRead );

                if ( mDenoiserDataBuffer.mHandle ) {
                    builder.UseResource( mDenoiserDataBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                    builder.UseResource( mPreviousPositionsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
                if ( mReconstructionDataBuffer.mHandle ) {
                    builder.UseResource( mReconstructionDataBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                    builder.UseResource( mReconstructionPositionsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
                for ( const auto handle: { mFrameConstantsBuffer, mViewEffectsBuffer, mSceneDataBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &context, core::Blackboard &b ) {
                // Zero tells the shader that this graph has no volume buffer.
                mFrameConstants.mVolumes = b.Contains<VolumeModuleInfo>()
                                               ? context.GetDeviceBufferAddress( b.Get<VolumeModuleInfo>().mData )
                                               : 0;
                mFrameUploads.Clear();

                if ( !mCanTraceFrame ) {
                    return;
                }

                try {
                    PrepareFrameUpload( context );
                } catch ( const std::exception &error ) {
                    RecordTraceError( error.what() );
                }
            } );
    }

    auto RayTracingModule::RegisterFrameTransferPass( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "RayTracing_UploadFrame",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mSceneAccelerationStructure, FGPipelineStage::eAccelStructRead, FGResourceAccess::eRead );

                if ( mDenoiserDataBuffer.mHandle ) {
                    builder.UseResource( mDenoiserDataBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                    builder.UseResource( mPreviousPositionsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
                if ( mReconstructionDataBuffer.mHandle ) {
                    builder.UseResource( mReconstructionDataBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                    builder.UseResource( mReconstructionPositionsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
                for ( const auto handle: { mFrameConstantsBuffer, mViewEffectsBuffer, mSceneDataBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                if ( !mCanTraceFrame ) {
                    return;
                }

                mFrameUploads.Submit( context );
                // Preparation only stages bytes. Keep them dirty if preparation or
                // transfer fails, and acknowledge residency after recording the copies.
                mPreviousPositions.mUploadPending = false;
                mReconstructionPositions.mUploadPending = false;
            } );
    }

    auto RayTracingModule::RegisterTracePass( FrameGraph &graph, bool pathTracing, bool atmosphere ) -> void {
        MKT_BEGIN_PROFILER();

        mTracePassUsesPathTracing = pathTracing;

        CreateTraceResources( graph );
        CreateTracePipeline( graph );

        graph.RegisterPass(
            "RayTracing_Lighting",
            FGPassType::eGraphics,
            [this, atmosphere]( FGNodeBuilder &builder, core::Blackboard &b ) {
                if ( const auto *volumes{ b.TryGet<VolumeModuleInfo>() } ) {
                    builder.UseResource( volumes->mData, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                }

                for ( const auto handle: { mShadingVertexBuffer, mAccelerationBuildPositionBuffer, mIndexBuffer,
                          mSceneDataBuffer, mViewEffectsBuffer, mFrameConstantsBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                }

                if ( mDenoiserDataBuffer.mHandle ) {
                    builder.UseResource( mDenoiserDataBuffer, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                    builder.UseResource( mPreviousPositionsBuffer, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                    for ( auto texture: mDenoiserInputs.Textures() ) {
                        builder.UseResource( texture, FGPipelineStage::eRayTracingShader, FGResourceAccess::eWrite );
                    }
                }
                if ( mReconstructionPipeline.mHandle ) {
                    builder.UseResource( mReconstructionDataBuffer, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                    builder.UseResource( mReconstructionPositionsBuffer, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                    for ( auto texture: mReconstructionInputs.GetTextures() ) {
                        builder.UseResource( texture, FGPipelineStage::eRayTracingShader, FGResourceAccess::eWrite );
                    }
                }
                builder.UseResource( mLightingHistory, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                builder.UseResource( mRadianceOutput, FGPipelineStage::eRayTracingShader, FGResourceAccess::eWrite );
                builder.UseResource( mAccumulatedLightingOutput, FGPipelineStage::eRayTracingShader, FGResourceAccess::eWrite );
                builder.UseResource( mOcclusionOutput, FGPipelineStage::eRayTracingShader, FGResourceAccess::eWrite );
                builder.UseResource( mOcclusionHistory, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                builder.UseResource( mSceneAccelerationStructure, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );

                if ( atmosphere ) {
                    const auto &sky{ b.Get<AtmosphereModuleInfo>() };

                    for ( const auto handle: { sky.mTransmittance, sky.mMultipleScattering, sky.mSkyView } ) {
                        builder.UseResource( handle, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                    }
                }
            },
            [this]( CommandContext &context, core::Blackboard &b ) {
                auto &info{ b.Get<RayTracingInfo>() };
                info.mShadowsEnabled = mConfiguration.mShadowsEnabled;
                info.mReflectionsEnabled = mConfiguration.mReflectionsEnabled;
                info.mGlobalIlluminationEnabled = mConfiguration.mGlobalIlluminationEnabled;
                info.mAmbientOcclusionEnabled = !mTracePassUsesPathTracing && mConfiguration.mAmbientOcclusionEnabled;

                if ( mCanTraceFrame ) {
                    try {
                        Trace( context );
                    } catch ( const std::exception &error ) {
                        RecordTraceError( error.what() );
                    }
                }

                info.mTraceOutputsReady = mCanTraceFrame;
                info.mReconstructionReady = mCanTraceFrame && mReconstructionPipeline.mHandle;
            } );
    }

    auto RayTracingModule::CreateTraceResources( FrameGraph &graph ) -> void {
        const auto width{ mDimensions.x };
        const auto height{ mDimensions.y };

        const auto texture{ [&]( cstr name ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( width )
                .SetHeight( height )
                .SetFormat( Format::eRGBA32_FLOAT )
                .SetUsage( TextureUsageFlagsBits::ShaderResource |
                               TextureUsageFlagsBits::UnorderedAccess |
                               TextureUsageFlagsBits::CopySource |
                               TextureUsageFlagsBits::CopyDest ) );
        } };

        mRadianceOutput = texture( "RayTracing_Output" );
        if ( mTracePassUsesPathTracing && mReconstructionDataBuffer.mHandle ) {
            auto fields{ mReconstructionInputs.GetTextures() };
            constexpr const char *names[]{ "NoisyHDR", "DiffuseAlbedo", "SpecularAlbedo", "NormalRoughness", "Depth", "Motion" };
            constexpr Format formats[]{ Format::eRGBA32_FLOAT, Format::eRGBA16_FLOAT, Format::eRGBA16_FLOAT,
                Format::eRGBA16_FLOAT, Format::eR32_FLOAT, Format::eRG16_FLOAT };
            for ( core::u32 index{}; index < fields.size(); ++index ) {
                fields[index] = graph.Create( FGTextureDescription{}
                    .SetName( string::Format( "DLSS_Input_{}", names[index] ) )
                    .SetWidth( width )
                    .SetHeight( height )
                    .SetFormat( formats[index] )
                    .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
            }
            mReconstructionInputs = { fields[0], fields[1], fields[2], fields[3], fields[4], fields[5] };
        }
#if MIKOTO_ENABLE_NRD
        if ( mDenoiserSettings.mEnabled ) {
            auto fields{ mDenoiserInputs.Textures() };
            const char *names[]{ "Diffuse", "Specular", "NormalRoughness", "ViewZ", "Motion", "DiffuseFactor", "SpecularFactor", "Base" };
            for ( core::u32 i{}; i < fields.size(); ++i ) {
                fields[i] = graph.Create( FGTextureDescription{}
                    .SetName( string::Format( "Denoiser_Input_{}", names[i] ) )
                    .SetWidth( width )
                    .SetHeight( height )
                    .SetFormat( i == 3 ? Format::eR32_FLOAT : i == 7 ? Format::eRGBA32_FLOAT
                                                                         : Format::eRGBA16_FLOAT )
                        .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
            }
            mDenoiserInputs = { fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7] };
        }
#endif
        mAccumulatedLightingOutput = texture( "RayTracing_Accumulation" );
        mLightingHistory = texture( "RayTracing_History" );
        mOcclusionOutput = texture( "RayTracing_AmbientOcclusion" );
        mOcclusionHistory = texture( "RayTracing_AmbientOcclusionHistory" );

        mMaterialSampler = graph.Create( FGSamplerDescription{}
            .SetName( "RayTracing_MaterialSampler" )
            .SetMipLevels( 32.0f )
            .SetMaxAnisotropy( 8.0f )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eRepeat ) );

        mAtmosphereSampler = graph.Create( FGSamplerDescription{}
            .SetName( "RayTracing_EnvironmentSampler" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) );

        auto &info{ graph.GetOrCreate<RayTracingInfo>() };
        info.mDenoiser = mDenoiserInputs;
        info.mReconstruction = mReconstructionInputs;
        info.mDirect = mRadianceOutput;
        info.mIndirect = mAccumulatedLightingOutput;
        info.mAmbientOcclusion = mOcclusionOutput;
    }

    auto RayTracingModule::CreateTracePipeline( FrameGraph &graph ) -> void {
        // Compilation creates the pipeline first, then its dependent shader table.
        FGPipelineDescription pipeline{};
        pipeline.SetName( "RayTracing_Pipeline" )
            .SetPipelineType( PipelineType::eRayTracing )
            .PushShader( mDenoiserDataBuffer.mHandle ? "PathTracing_NRD_RayGen.slang" : "PathTracing_RayGen.slang", FGStageType::eRayGen )
            .PushShader( "PathTracing_Miss.slang", FGStageType::eMiss )
            .PushShader( "PathTracing_ClosestHit.slang", FGStageType::eClosestHit );

        // The hit payload includes position uncertainty for robust secondary rays.
        pipeline.mRayPayloadBytes = kRayHitPayloadBytes;
        mTracePipeline = graph.Create( pipeline );
        mTraceShaderTable = graph.CreateShaderTable( mTracePipeline );
        if ( mReconstructionInputs.mNoisyColor.mHandle ) {
            FGPipelineDescription reconstruction{};
            reconstruction.SetName( "DLSS_InputPipeline" )
                .SetPipelineType( PipelineType::eRayTracing )
                .PushShader( "PathTracing_DLSS_RayGen.slang", FGStageType::eRayGen )
                .PushShader( "PathTracing_Miss.slang", FGStageType::eMiss )
                .PushShader( "PathTracing_ClosestHit.slang", FGStageType::eClosestHit );
            reconstruction.mRayPayloadBytes = kRayHitPayloadBytes;
            mReconstructionPipeline = graph.Create( reconstruction );
            mReconstructionShaderTable = graph.CreateShaderTable( mReconstructionPipeline );
        }
    }

    auto RayTracingModule::RegisterHistoryPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        graph.RegisterPass(
            "RayTracing_UpdateHistory",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mAccumulatedLightingOutput, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( mLightingHistory, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mOcclusionOutput, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( mOcclusionHistory, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                UpdateHistory( context );
            } );
    }

    auto RayTracingModule::UpdateHistory( CommandContext &context ) -> void {
        if ( !mCanTraceFrame ) {
            return;
        }

        // Raygen reads last frame's history while writing this frame's accumulation.
        const auto width{ mDimensions.x };
        const auto height{ mDimensions.y };
        const TextureSlice slice{
            .mWidth = as<u32>( width ),
            .mHeight = as<u32>( height ),
            .mDepth = 1
        };
        if ( !mTracePassUsesPathTracing || !mReconstructionDenoising ) {
            // RR owns radiance history; AO debug keeps its separate history below.
            context.Copy( mAccumulatedLightingOutput, slice, mLightingHistory, slice );
        }

        if ( IsOcclusionActive() ) {
            context.Copy( mOcclusionOutput, slice, mOcclusionHistory, slice );
            mAccumulatedOcclusionSamples = eastl::min( kOcclusionHistoryLimit, mAccumulatedOcclusionSamples + mConfiguration.mAoSamplesPerFrame );
        }

        if ( mTracePassUsesPathTracing ) {
            if ( !mReconstructionDenoising ) {
                mAccumulatedRadianceSamples = eastl::min( mConfiguration.mPathMaxSamples, mAccumulatedRadianceSamples + mActivePathSamples );
            }
        }
        else if ( mConfiguration.mGlobalIlluminationEnabled || mConfiguration.mReflectionsEnabled ) {
            const auto samples{ mConfiguration.mRayGiSamplesPerFrame };
            mAccumulatedRadianceSamples = eastl::min( kHybridHistoryLimit, mAccumulatedRadianceSamples + samples );
        }

        ++mTraceFrameIndex;
    }

    auto RayTracingModule::IsOcclusionActive() const -> bool {
        return ( !mTracePassUsesPathTracing && mConfiguration.mAmbientOcclusionEnabled ) || mAmbientOcclusionDebugEnabled;
    }

    auto RayTracingModule::PrepareGeometryUpload( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER();

        // Reuse device-local storage; graph read/write barriers protect earlier ray consumers.
        const auto upload{ [&]( FGBufferHandle handle, const auto &values ) {
            const usize bytes{ values.size() * sizeof( values[0] ) };
            context.EnsureBufferCapacity( handle, bytes );

            mGeometryUploads.Copy( handle, 0, values.data(), bytes );
        } };

        upload( mShadingVertexBuffer, mPackedScene.mVertices );
        upload( mAccelerationBuildPositionBuffer, mPackedScene.mBuildPositions );
        upload( mIndexBuffer, mPackedScene.mIndices );
    }

    auto RayTracingModule::PrepareFrameUpload( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER();

        const auto data{ ResolveSceneTextures( context ) };
        context.EnsureBufferCapacity( mSceneDataBuffer, data.size() );

        mFrameUploads.Copy( mSceneDataBuffer, 0, data.data(), data.size() );
        mFrameUploads.Copy( mViewEffectsBuffer, mViewEffects, 0 );
        if ( mDenoiserDataBuffer.mHandle ) {
            PreparePositionHistoryUpload( context, mPreviousPositionsBuffer, mPreviousPositions );
            auto images{ mDenoiserInputs.Textures() };
            for ( core::u32 i{}; i < 4; ++i ) {
                mDenoiserData.mImages0[i] = context.PushUAV( images[i] );
                mDenoiserData.mImages1[i] = context.PushUAV( images[i + 4] );
            }
            mDenoiserData.mPreviousPositions = context.GetDeviceBufferAddress( mPreviousPositionsBuffer );
            mDenoiserData.mEnabled = mDenoiserSettings.mEnabled;
            mDenoiserData.mMethod = static_cast<core::u32>( mDenoiserSettings.mType );
            mDenoiserData.mJitter.z = static_cast<float>( mAccumulatedRadianceSamples );
            mFrameUploads.Copy( mDenoiserDataBuffer, mDenoiserData, 0 );
        }

        PrepareReconstructionUpload( context );
        BindFrameResources( context );
        mFrameUploads.Copy( mFrameConstantsBuffer, mFrameConstants, 0 );
    }

    auto RayTracingModule::CapturePositionHistory( PositionHistory &history ) -> void {
        if ( history.mGeometryRevision == mGeometryRevision ) {
            return;
        }

        // Camera movement never repacks the scene. Avoid gathering millions of
        // unchanged world positions just to update the previous camera matrix.
        history.mPositions.resize( mPackedScene.mVertices.size() );
        for ( core::usize index{}; index < mPackedScene.mVertices.size(); ++index ) {
            history.mPositions[index] = mPackedScene.mVertices[index].mPosition;
        }
        history.mGeometryRevision = mGeometryRevision;
        history.mTopologyRevision = mTopologyRevision;
        history.mUploadPending = true;
    }

    auto RayTracingModule::PreparePositionHistoryUpload( CommandContext &context, FGBufferHandle buffer, PositionHistory &history ) -> void {
        // Replaced/reordered geometry has no valid previous vertex correspondence,
        // even when its vertex count is unchanged. Start it at its current pose.
        if ( history.mGeometryRevision == 0 || history.mTopologyRevision != mTopologyRevision ) {
            CapturePositionHistory( history );
        }

        const auto bytes{ history.mPositions.size() * sizeof( core::float4 ) };
        const bool replaced{ context.EnsureBufferCapacity( buffer, bytes ) };
        history.mUploadPending |= replaced;
        if ( history.mUploadPending ) {
            // Upload once per captured pose (or fresh storage), not once per view
            // update. Transfer and ray-read dependencies remain owned by the graph.
            mFrameUploads.Copy( buffer, 0, history.mPositions.data(), bytes );
        }
    }

    auto RayTracingModule::PrepareReconstructionUpload( CommandContext &context ) -> void {
        if ( !mReconstructionPipeline.mHandle ) {
            return;
        }
        // Camera cuts discard reprojection, but a stationary mesh still has the same
        // positions. Refresh that snapshot only if the packed geometry also changed.
        if ( !mReconstructionHistoryValid ) {
            CapturePositionHistory( mReconstructionPositions );
        }
        PreparePositionHistoryUpload( context, mReconstructionPositionsBuffer, mReconstructionPositions );
        const auto images{ mReconstructionInputs.GetTextures() };
        for ( core::u32 index{}; index < images.size(); ++index ) {
            if ( index < mReconstructionData.mImages0.length() ) {
                mReconstructionData.mImages0[index] = context.PushUAV( images[index] );
            }
            else {
                mReconstructionData.mImages1[index - mReconstructionData.mImages0.length()] = context.PushUAV( images[index] );
            }
        }
        const auto width{ mDimensions.x };
        const auto height{ mDimensions.y };
        mReconstructionData.mWidth = static_cast<core::u32>( width );
        mReconstructionData.mHeight = static_cast<core::u32>( height );
        mReconstructionData.mPreviousPositions = context.GetDeviceBufferAddress( mReconstructionPositionsBuffer );
        mReconstructionData.mGuideOptions.x = mDenoiserSettings.mDlssClearGlass ? 1u : 0u;
        mReconstructionData.mViewProjection = glm::inverse( mFrameConstants.mInverseViewProjection );
        mReconstructionData.mPreviousViewProjection = mReconstructionHistoryValid
            ? mPreviousReconstructionViewProjection : mReconstructionData.mViewProjection;

        // Halton film jitter follows the reference renderer; matrices remain
        // unjittered so motion vectors describe motion rather than sample offsets.
        constexpr core::u32 jitterPeriod{ 1024 };
        const auto halton{ []( core::u32 index, core::u32 base ) {
            core::f32 value{};
            core::f32 weight{ 1.0f };
            while ( index != 0 ) {
                weight /= static_cast<core::f32>( base );
                value += weight * static_cast<core::f32>( index % base );
                index /= base;
            }
            return value;
        } };
        const auto index{ mTraceFrameIndex % jitterPeriod + 1u };
        mReconstructionData.mJitter = core::float4{ halton( index, 2u ) - .5f, halton( index, 3u ) - .5f,
            mReconstructionHistoryValid ? 1.0f : 0.0f, 0.0f };
        mFrameUploads.Copy( mReconstructionDataBuffer, mReconstructionData, 0 );
    }

    auto RayTracingModule::ResolveSceneTextures( CommandContext &context ) const -> eastl::vector<ubyte> {
        // Keep local texture indices in the cached scene. Resolve graph descriptors each frame.
        eastl::vector<u32> textureIndices{};
        textureIndices.reserve( mPackedScene.mTextures.size() );

        for ( const auto &texture: mPackedScene.mTextures ) {
            const auto handle{ context.ImportSampledTexture( texture ) };
            textureIndices.push_back( context.UseImportedTexture( handle ) );
        }

        auto data{ mPackedScene.mSceneData };
        SceneHeader header{};
        std::memcpy( &header, data.data(), sizeof( header ) );

        const auto resolve{ [&]( i32 index ) -> i32 {
            return index < 0 ? kInvalidTextureIndex : as<i32>( textureIndices.at( as<usize>( index ) ) );
        } };

        header.mEnvironmentIndex = resolve( header.mEnvironmentIndex );

        // The CPU header stores the first face slot. Raygen expects a byte offset.
        if ( header.mHasFaces ) {
            const auto firstFace{ header.mFaceOffset };
            header.mFaceOffset = as<u32>( data.size() );

            for ( usize face{}; face < mEnvironmentFaces.size(); ++face ) {
                const auto descriptor{ textureIndices.at( firstFace + face ) };
                AppendSceneBytes( data, &descriptor, sizeof( descriptor ) );
            }
        }

        std::memcpy( data.data(), &header, sizeof( header ) );

        for ( u32 index{}; index < header.mMaterialCount; ++index ) {
            Material material{};
            const usize offset{ sizeof( header ) + index * sizeof( Material ) };
            std::memcpy( &material, data.data() + offset, sizeof( material ) );

            for ( auto map{ 0 }; map < material.mTextures.length(); ++map ) {
                material.mTextures[map] = resolve( material.mTextures[map] );
            }

            material.mHeightInfo.x = resolve( material.mHeightInfo.x );
            std::memcpy( data.data() + offset, &material, sizeof( material ) );
        }

        return data;
    }

    auto RayTracingModule::BindFrameResources( CommandContext &context ) -> void {
        // Scene arrays use buffer addresses. Textures, samplers and the TLAS use descriptor indices.
        mFrameConstants.mDenoiser = mDenoiserDataBuffer.mHandle ? context.GetDeviceBufferAddress( mDenoiserDataBuffer ) : 0;
        mFrameConstants.mReconstruction = mReconstructionPipeline.mHandle ? context.GetDeviceBufferAddress( mReconstructionDataBuffer ) : 0;
        mFrameConstants.mVertices = context.GetDeviceBufferAddress( mShadingVertexBuffer );
        mFrameConstants.mBuildPositions = context.GetDeviceBufferAddress( mAccelerationBuildPositionBuffer );
        mFrameConstants.mIndices = context.GetDeviceBufferAddress( mIndexBuffer );
        mFrameConstants.mData = context.GetDeviceBufferAddress( mSceneDataBuffer );
        mFrameConstants.mViewEffects = context.GetDeviceBufferAddress( mViewEffectsBuffer );

        mFrameConstants.mOutput = context.PushUAV( mRadianceOutput );
        mFrameConstants.mAuxiliary = context.PushUAV( mAccumulatedLightingOutput );
        mFrameConstants.mHistory = context.PushSRV( mLightingHistory );
        mFrameConstants.mOcclusionOutput = context.PushUAV( mOcclusionOutput );
        mFrameConstants.mOcclusionHistory = context.PushSRV( mOcclusionHistory );
        mFrameConstants.mOcclusion = glm::uvec4{ IsOcclusionActive(),
            mConfiguration.mAoSamplesPerFrame, mAccumulatedOcclusionSamples, mConfiguration.mUseCosineWeightedAoSampling };
        mFrameConstants.mOcclusionMaxDistance = mConfiguration.mAoMaxDistance > 0.0f ? mConfiguration.mAoMaxDistance : 1e30f;
        mFrameConstants.mLightDistribution = mPackedScene.mLightDistribution;
        mFrameConstants.mMaterialSampler = context.PushSampler( mMaterialSampler );
        mFrameConstants.mAtmosphereSampler = context.PushSampler( mAtmosphereSampler );
        mFrameConstants.mSceneIndex = context.PushSRV( mSceneAccelerationStructure );

        for ( usize index{}; index < mAtmosphereTextures.size(); ++index ) {
            const auto &texture{ mAtmosphereTextures[index] };
            mFrameConstants.mAtmosphereTextures[index] = texture
                                                             ? context.UseImportedTexture( context.ImportSampledTexture( texture ) )
                                                             : 0;
        }

        mFrameConstants.mFrame = glm::uvec4{
            mTraceFrameIndex,
            mTracePassUsesPathTracing ? mActivePathSamples : mConfiguration.mRayGiSamplesPerFrame,
            mTracePassUsesPathTracing ? mConfiguration.mPathMaxBounces : mConfiguration.mRayGiMaxBounces,
            mTracePassUsesPathTracing ? 1u : 0u
        };
        mFrameConstants.mOptions = glm::uvec4{ mDenoiserDataBuffer.mHandle && mDenoiserSettings.mEnabled ? 0u : mAccumulatedRadianceSamples, mConfiguration.mShadowsEnabled ? mConfiguration.mShadowSamplesPerFrame : 0u,
            ( mConfiguration.mReflectionsEnabled ? 1u : 0u ) | ( mConfiguration.mGlobalIlluminationEnabled ? 2u : 0u ),
            mTracePassUsesPathTracing ? mConfiguration.mPathMaxSamples : mConfiguration.mReflectionMaxBounces };
    }

    auto RayTracingModule::Trace( CommandContext &context ) -> void {
        // Frame preparation has already resolved the TLAS index written to the shader constants.
        ( void )context.PushSRV( mSceneAccelerationStructure );

        const auto frameAddress{ context.GetDeviceBufferAddress( mFrameConstantsBuffer ) };
        const auto width{ mDimensions.x };
        const auto height{ mDimensions.y };

        // RR's ray generator produces the fresh color and guides in one dispatch.
        // Diagnostic previews alongside NRD/OIDN still keep the ordinary beauty path.
        if ( !mTracePassUsesPathTracing || !mReconstructionDenoising || IsOcclusionActive() ) {
            context.BindPipeline( mTracePipeline );
            context.BindShaderTable( mTraceShaderTable );
            context.PushConstants( frameAddress );
            context.DispatchRays( width, height );
            if ( mDenoiserDataBuffer.mHandle ) {
                CapturePositionHistory( mPreviousPositions );
            }
        }
        if ( mReconstructionPipeline.mHandle ) {
            // This diagnostic dispatch has its own current-frame color contract.
            // It never reads/writes the progressive or NRD/OIDN beauty histories.
            context.BindPipeline( mReconstructionPipeline );
            context.BindShaderTable( mReconstructionShaderTable );
            context.PushConstants( frameAddress );
            context.DispatchRays( width, height );
            // Advance only after tracing this pose, never on a skipped frame.
            // A final upload on the next frame removes motion when an object stops;
            // later stationary frames reuse both the CPU snapshot and GPU buffer.
            CapturePositionHistory( mReconstructionPositions );
            mPreviousReconstructionViewProjection = mReconstructionData.mViewProjection;
            mReconstructionHistoryValid = true;
        }
    }

    auto RayTracingConfiguration::Sanitize() -> void {
        mShadowSamplesPerFrame = glm::clamp( mShadowSamplesPerFrame, 1u, 16u );
        mReflectionMaxBounces = glm::clamp( mReflectionMaxBounces, 1u, 8u );
        mRayGiSamplesPerFrame = glm::clamp( mRayGiSamplesPerFrame, 1u, 8u );
        mRayGiMaxBounces = glm::clamp( mRayGiMaxBounces, 1u, 8u );
        mAoSamplesPerFrame = glm::clamp( mAoSamplesPerFrame, 1u, 64u );
        mAoMaxDistance = glm::max( mAoMaxDistance, 0.0f );
        mPathSamplesPerFrame = glm::clamp( mPathSamplesPerFrame, 1u, kMaxPathSamplesPerFrame );
        mPathMaxBounces = glm::clamp( mPathMaxBounces, 1u, 32u );
        mPathMaxSamples = glm::clamp( mPathMaxSamples, 1u, 1048576u );
    }

    auto RayTracingModule::Prepare( FrameGraph &, const scene::Scene *scene, const scene::Camera *camera, const RayTracingConfiguration &settings,
        rhi::TextureHandle environment, float environmentScale, float exposure, float gamma ) -> void {
        mCanTraceFrame = false;
        // Ordinary camera motion keeps the previous rendered view. A different
        // camera or projection type has no compatible reprojection history.
        const bool orthographic{ camera && camera->IsOrthographic() };
        if ( mReconstructionCamera != camera || mReconstructionOrthographic != orthographic ) {
            mReconstructionHistoryValid = false;
            mReconstructionCamera = camera;
            mReconstructionOrthographic = orthographic;
        }
        if ( camera ) {
            mDenoiserData.mWorldToView = camera->GetViewMatrix();
        }

        // AO has its own history and can change independently of radiance.
        if ( mConfiguration.mAmbientOcclusionEnabled != settings.mAmbientOcclusionEnabled ||
             mConfiguration.mUseCosineWeightedAoSampling != settings.mUseCosineWeightedAoSampling ||
             mConfiguration.mAoMaxDistance != settings.mAoMaxDistance ) {
            mAccumulatedOcclusionSamples = 0;
        }

        mConfiguration = settings;

        const bool hasRayWork{ mTracePassUsesPathTracing || settings.mShadowsEnabled || settings.mReflectionsEnabled ||
                               settings.mGlobalIlluminationEnabled || settings.mAmbientOcclusionEnabled || mAmbientOcclusionDebugEnabled };

        if ( !camera || !hasRayWork ) {
            ResetAccumulation();
            return;
        }

        try {
            PrepareView( *camera, environmentScale, exposure, gamma );
            PrepareScene( scene, environment );
            mCanTraceFrame = true;
            mLastTraceError.clear();
        } catch ( const std::exception &error ) {
            RecordTraceError( error.what() );
        }
    }

    auto RayTracingModule::RecordTraceError( const char *message ) -> void {
        if ( mLastTraceError != message ) {
            MKT_CORE_LOGGER_ERROR( "Ray tracing: {}", message );
        }

        mLastTraceError = message;
        mCanTraceFrame = false;
    }

    auto RayTracingModule::IsReady() const -> bool {
        return mCanTraceFrame;
    }

    auto RayTracingModule::SetDenoiserSettings( const DenoiserSettings &settings ) -> void {
        auto transportSettings{ settings };
        mReconstructionDenoising = settings.mEnabled && settings.mBackend == DenoiserBackend::eDlss;
        transportSettings.mReconstructionGuides |= mReconstructionDenoising;
        transportSettings.mEnabled &= settings.mBackend == DenoiserBackend::eNrd;
        if ( mDenoiserSettings.mEnabled != transportSettings.mEnabled || mDenoiserSettings.mBackend != settings.mBackend ) {
            ResetAccumulation();
        }
        mDenoiserSettings = transportSettings;
    }

    auto RayTracingModule::ResetReconstructionHistory() -> void {
        mReconstructionHistoryValid = false;
    }

    auto RayTracingModule::GetReconstructionData() const -> const DlssRayData & {
        return mReconstructionData;
    }

    auto RayTracingModule::ResetAccumulation() -> void {
        mAccumulatedRadianceSamples = 0;
        mAccumulatedOcclusionSamples = 0;
    }

    auto RayTracingModule::SetAmbientOcclusionDebug( bool enabled ) -> void {
        if ( enabled != mAmbientOcclusionDebugEnabled ) {
            mAccumulatedOcclusionSamples = 0;
            mAmbientOcclusionDebugEnabled = enabled;
        }
    }

    auto RayTracingModule::GetSampleCount() const -> core::u32 {
        return mAccumulatedRadianceSamples;
    }

    auto RayTracingModule::GetError() const -> const eastl::string & {
        return mLastTraceError;
    }

    auto RayTracingModule::SetBackground( SceneBackgroundType type, core::f32 blur ) -> void {
        mViewEffects.mBackground = core::float4{
            glm::clamp( blur, 0.0f, 16.0f ),
            type == SceneBackgroundType::ePrefilterMap ? 1.0f : 0.0f,
            type == SceneBackgroundType::eClearColor ? 1.0f : 0.0f, 0.0f
        };
    }

    auto RayTracingModule::SetAtmosphere( const AtmosphereData *data, const eastl::array<renderer::rhi::TextureHandle, 3> &textures ) -> void {
        const bool enabled{ data != nullptr };

        if ( enabled != ( mViewEffects.mControl.x != 0.0f ) ||
             ( enabled && std::memcmp( &mViewEffects.mAtmosphere, data, sizeof( AtmosphereData ) ) != 0 ) ) {
            ResetAccumulation();
        }

        mViewEffects.mControl.x = enabled ? 1.0f : 0.0f;
        mViewEffects.mAtmosphere = enabled ? *data : AtmosphereData{};
        mAtmosphereTextures = textures;
    }

    auto RayTracingModule::SetMeshesVisible( bool visible ) -> void {
        mMeshesVisible = visible;
    }

    auto RayTracingModule::SetEnvironmentFaces( const eastl::array<rhi::TextureHandle, 6> &textures ) -> void {
        mEnvironmentFaces = textures;
    }

    auto RayTracingModule::PrepareView( const scene::Camera &camera, f32 environmentScale, f32 exposure, f32 gamma ) -> void {
        if ( mConfiguration.mPathTracingEnabled && mAccumulatedRadianceSamples > mConfiguration.mPathMaxSamples ) {
            mAccumulatedRadianceSamples = 0;
        }

        const auto inverse{ glm::inverse( camera.GetViewProjection() ) };

        // PBRT's perspective camera samples a finite aperture and aims at a focus plane.
        // https://pbr-book.org/4ed/Cameras_and_Film/Projective_Camera_Models
        mViewEffects.mLensUp = {};
        mViewEffects.mLensRight = {};
        mViewEffects.mLensForward = {};

        const auto &lens{ camera.GetDepthOfFieldSettings() };
        if ( mConfiguration.mPathTracingEnabled && lens.mEnabled && !camera.IsOrthographic() ) {
            const auto pose{ glm::inverse( camera.GetViewMatrix() ) };
            const f32 focalLength{ glm::clamp( lens.mFocalLength, 1.0f, 300.0f ) * 0.001f };
            const f32 aperture{ glm::max( lens.mAperture, glm::max( lens.mMaxAperture, 0.7f ) ) };
            const f32 focusDistance{ glm::max( lens.mFocusDistance, focalLength + 0.001f ) };
            mViewEffects.mLensRight = float4{ glm::normalize( float3{ pose[0] } ), focalLength / ( 2.0f * aperture ) };
            mViewEffects.mLensUp = float4{ glm::normalize( float3{ pose[1] } ), focusDistance };
            mViewEffects.mLensForward = float4{ -glm::normalize( float3{ pose[2] } ), 0.0f };
        }

        // Camera, lens and transport changes invalidate the radiance averaged into this view.
        u64 viewHash{ kRevisionHashSeed };
        Hash( viewHash, inverse );
        Hash( viewHash, mConfiguration.mPathMaxBounces );
        Hash( viewHash, mConfiguration.mPathTracingEnabled );
        Hash( viewHash, mConfiguration.mShadowsEnabled );
        Hash( viewHash, mConfiguration.mReflectionsEnabled );
        Hash( viewHash, mConfiguration.mGlobalIlluminationEnabled );
        Hash( viewHash, mConfiguration.mShadowSamplesPerFrame );
        Hash( viewHash, mConfiguration.mReflectionMaxBounces );
        Hash( viewHash, mConfiguration.mRayGiSamplesPerFrame );
        Hash( viewHash, mConfiguration.mRayGiMaxBounces );
        Hash( viewHash, environmentScale );
        Hash( viewHash, core::float3{ mViewEffects.mBackground } );
        Hash( viewHash, mViewEffects.mLensRight );
        Hash( viewHash, mViewEffects.mLensUp );
        Hash( viewHash, mViewEffects.mLensForward );

        // The transport's enabled flag is NRD-only; RR owns a separate temporal
        // history and needs the same navigation budget. OIDN remains progressive.
        // Moving views use one sample; stationary views retain the requested SPP.
        const bool temporalDenoising{ mDenoiserSettings.mEnabled || mReconstructionDenoising };
        mActivePathSamples = temporalDenoising && mDenoiserSettings.mAdaptiveSampling && viewHash != mAccumulationViewHash
                                 ? 1u
                                 : mConfiguration.mPathSamplesPerFrame;

        if ( viewHash != mAccumulationViewHash ) {
            ResetAccumulation();
            mAccumulationViewHash = viewHash;
        }

        mFrameConstants.mInverseViewProjection = inverse;
        mFrameConstants.mCamera = core::float4{ camera.GetPosition(), camera.GetProjectionType() == scene::ProjectionType::eOrthographic ? 1.0f : 0.0f };
        mFrameConstants.mDisplay = core::float2{ exposure, gamma };

        mViewEffects.mBackground.w = environmentScale;

        mViewEffects.mControl.z = camera.GetNearPlane();
        mViewEffects.mControl.w = camera.GetFarPlane();
    }

    auto RayTracingModule::PrepareScene( const scene::Scene *scene, const TextureHandle &environment ) -> void {
        SceneInputs inputs{};
        CollectEnvironment( environment, inputs );

        // Camera changes reset accumulation independently of the packed scene.
        Hash( inputs.mHash, scene );
        Hash( inputs.mHash, mMeshesVisible );
        Hash( inputs.mHash, mConfiguration.mPathTracingEnabled );
        Hash( inputs.mHash, inputs.mEnvironmentIndex );
        Hash( inputs.mHash, inputs.mCubeEnvironment );
        Hash( inputs.mHash, mViewEffects.mControl.x );
        if ( mViewEffects.mControl.x != 0 ) {
            Hash( inputs.mHash, mViewEffects.mAtmosphere.mSunDirection );
            Hash( inputs.mHash, mViewEffects.mAtmosphere.mSunIlluminance );
        }

        if ( scene ) {
            CollectMeshes( *scene, inputs );
            CollectLights( *scene, inputs );
        }

        // Membership, mesh replacement and index ordering can change while the
        // total vertex count stays the same. Include correspondence in the cache key.
        Hash( inputs.mHash, inputs.mTopologyHash );

        // Lighting changes need responsive histories, not new vertex/index data.
        Hash( inputs.mLightingHash, scene );
        Hash( inputs.mLightingHash, mViewEffects.mBackground.w );
        Hash( inputs.mLightingHash, mViewEffects.mAtmosphere.mSunDirection );
        Hash( inputs.mLightingHash, mViewEffects.mAtmosphere.mSunIlluminance );
        for ( const auto &light: inputs.mLights ) {
            Hash( inputs.mLightingHash, light );
        }
        for ( const auto &mesh: inputs.mMeshes ) {
            Hash( inputs.mLightingHash, mesh.mMaterial.mEmission );
        }
        if ( mLightingRevision != inputs.mLightingHash ) {
            ResetAccumulation();
            mLightingRevision = inputs.mLightingHash;
        }

        for ( const auto &texture: inputs.mTextures ) {
            Hash( inputs.mHash, texture.GetPtr() );
        }

        if ( !mPackedScene.mIndices.empty() && inputs.mHash == mPackedSceneHash ) {
            return;
        }

        const bool topologyChanged{ inputs.mTopologyHash != mTopologyHash || scene != mDenoiserScene };
        mPackedScene = PackScene( inputs );
        // This revision advances on a repack, independently of camera/history
        // resets, so static views keep their existing previous-position buffers.
        ++mGeometryRevision;
        if ( topologyChanged ) {
            mReconstructionHistoryValid = false;
            ++mTopologyRevision;
        }
        mTopologyHash = inputs.mTopologyHash;
        mDenoiserScene = scene;
        mGeometryUploadPending = true;
        mPackedSceneHash = inputs.mHash;

        ResetAccumulation();
    }

    auto RayTracingModule::CollectEnvironment( const TextureHandle &environment, SceneInputs &inputs ) const -> void {
        inputs.mEnvironmentIndex = inputs.AddTexture( environment );
        inputs.mCubeEnvironment = !environment;

        for ( const auto &face: mEnvironmentFaces ) {
            inputs.mCubeEnvironment &= face && face->GetDimension() == TextureDimension::eTexture2D;
        }

        inputs.mEnvironmentFaceBase = as<u32>( inputs.mTextures.size() );

        if ( inputs.mCubeEnvironment ) {
            // Raygen expects six contiguous entries, including duplicate face textures.
            for ( const auto &face: mEnvironmentFaces ) {
                inputs.mTextureIndices.try_emplace( face.GetPtr(), as<i32>( inputs.mTextures.size() ) );
                inputs.mTextures.push_back( face );
            }
        }
    }

    auto RayTracingModule::CollectMeshes( const scene::Scene &scene, SceneInputs &inputs ) const -> void {
        if ( !mMeshesVisible ) {
            return;
        }

        const auto &registry{ scene.GetRegistry() };
        const auto windField{ scene.GetWindField() };
        const bool tracksGeometryHistory{ mDenoiserDataBuffer.mHandle || mReconstructionDataBuffer.mHandle };

        for ( const auto [entity, tag, transform, mesh, material]:
            registry.view<scene::TagComponent, scene::TransformComponent, scene::MeshComponent, scene::MaterialComponent>().each() ) {
            if ( !tag.IsActive() || !mesh.HasMesh() || !material.HasMaterial() ) {
                continue;
            }

            // A camera capture takes precedence over video and the authored material.
            const auto *videoComponent{ registry.try_get<scene::VideoComponent>( entity ) };
            const auto videoData{ videoComponent ? videoComponent->GetRenderData() : VideoRenderDataHandle{} };
            const auto videoMaterial{ videoData ? videoData->GetMaterial() : material::MaterialHandle{} };
            const auto *capture{ registry.try_get<scene::TextureRenderComponent>( entity ) };
            const auto capturedMaterial{ capture ? capture->GetMaterial() : material::MaterialHandle{} };
            auto renderMaterial{ capturedMaterial ? capturedMaterial : videoMaterial ? videoMaterial
                                                                                     : material.GetMaterialReference() };
            auto *physical{ dynamic_cast<material::SurfaceMaterial *>( renderMaterial.GetPtr() ) };

            if ( !physical ) {
                continue;
            }

            MeshInstance item{};
            const auto *node{ mesh.GetMesh() };
            const auto *deformed{ registry.try_get<scene::DeformedMeshComponent>( entity ) };
            item.mMesh = deformed && deformed->mMesh ? MeshRenderDataHandle{ deformed->mMesh } : node->GetRenderData();

            if ( !item.mMesh || item.mMesh->GetIndices().empty() ) {
                continue;
            }

            item.mTransform = transform.GetWorldTransform();

            // Skinning yields posed mesh-local positions for the BLAS.
            if ( const auto *skin{ registry.try_get<scene::SkinnedMeshRenderer>( entity ) }; skin && item.mMesh->IsSkinned() ) {
                const auto *animator{ animation::AnimationSystem::Get()->GetAnimator( skin->GetAnimatorID() ) };

                if ( animator ) {
                    const auto &pose{ animator->GetJointModelMatrices( node->GetSkinBinding().mRigIndex ) };
                    item.mPalette.resize( asset::kMaxBonesPerMesh );

                    if ( !animation::BuildSkinningPalette( node->GetSkinBinding(), { pose.data(), pose.size() },
                             { item.mPalette.data(), item.mPalette.size() } ) ) {
                        item.mPalette.clear();
                    }
                }
            }

            if ( item.mPalette.empty() ) {
                item.mTransform *= node->GetSkinBinding().mBindPoseTransform;
            }

            item.mMaterialSource = physical;
            item.mMaterial = PackMaterial( *physical, inputs );
            auto &materialRecord{ item.mMaterial };

            if ( const auto *area{ mConfiguration.mPathTracingEnabled ? FindMeshAreaLight( scene, entity ) : nullptr } ) {
                // Reuse the mesh's actual posed triangles and texture UVs. The light supplies
                // radiance; the material still supplies scattering, alpha and the emission map.
                materialRecord.mEmission = float4{ glm::max( area->GetColor() * area->GetIntensity(), float3{ 0 } ),
                    area->IsTwoSided() ? 2.0f : 1.0f };
                item.mMeshEmitter = true;
            }

            // Use the same scene time, root and controls as raster foliage. Baking
            // the displaced vertices also gives shadows/reflections the moving surface.
            const auto &foliage{ physical->GetFoliageWind() };
            if ( foliage.mEnabled && !item.mMesh->IsSkinned() ) {
                const auto root{ float3{ item.mTransform * float4{ 0, foliage.mRootHeight, 0, 1 } } };
                const auto velocity{ scene::SampleWind( windField, root ) };
                // A calm field keeps the ordinary static BLAS cached.
                if ( velocity != float3{} ) {
                    item.mFoliageMotion = { glm::clamp( foliage.mHeight, .01f, 100.0f ), glm::clamp( foliage.mTrunkBend, 0.0f, .25f ),
                        glm::clamp( foliage.mBranchMotion, 0.0f, 2.0f ), glm::clamp( foliage.mLeafFlutter, 0.0f, .5f ) };
                    item.mFoliageShape = { foliage.mRootHeight, glm::clamp( foliage.mFrequency, 0.0f, 10.0f ), 0.0f, 0.0f };
                    item.mFoliageWind = { velocity, windField.mTime };
                }
            }

            // Local shape identity excludes shading and rigid motion for static
            // meshes. Wind depends on world placement, so its BLAS includes that state.
            item.mShapeRevision = kRevisionHashSeed;
            Hash( item.mShapeRevision, item.mMesh.GetPtr() );
            Hash( item.mShapeRevision, item.mMesh->GetRevision() );
            Hash( item.mShapeRevision, item.mMesh->GetTopologyHash() );
            HashBytes( item.mShapeRevision, item.mPalette.data(), item.mPalette.size() * sizeof( float4x4 ) );
            const bool mirrored{ glm::determinant( glm::mat3{ item.mTransform } ) < 0.0f };
            Hash( item.mShapeRevision, mirrored );
            if ( item.mFoliageMotion.x > 0.0f ) {
                Hash( item.mShapeRevision, item.mFoliageMotion );
                Hash( item.mShapeRevision, item.mFoliageShape );
                Hash( item.mShapeRevision, item.mFoliageWind );
                Hash( item.mShapeRevision, item.mTransform );
            }

            const auto &height{ physical->GetHeightMapping() };

            if ( height.mMode == material::HeightMappingMode::eDisplacement && physical->IsTextureUsed( material::MapType::eHeight ) ) {
                Hash( item.mShapeRevision, materialRecord.mHeightMapping );
                Hash( item.mShapeRevision, materialRecord.mUvTransform );
                Hash( item.mShapeRevision, materialRecord.mHeightInfo.y );
                Hash( item.mShapeRevision, physical->GetHeightImage().GetPtr() );
            }

            // Keep radiance accumulation sensitive to pose, placement and material edits.
            Hash( inputs.mHash, item.mShapeRevision );
            Hash( inputs.mHash, item.mTransform );
            Hash( inputs.mHash, materialRecord );

            if ( capturedMaterial ) {
                // A capture can update pixels without changing its bindless texture index.
                Hash( inputs.mLightingHash, capture->GetTextureRevision() );
            }

            // RR and NRD both interpolate previous vertices using current triangle
            // indices. Deformation/rigid movement preserve this mapping; swaps do not.
            if ( tracksGeometryHistory ) {
                Hash( inputs.mTopologyHash, entity );
                Hash( inputs.mTopologyHash, item.mMesh.GetPtr() );
                Hash( inputs.mTopologyHash, mirrored );
                Hash( inputs.mTopologyHash, item.mMesh->GetVertexCount() );
                Hash( inputs.mTopologyHash, item.mMesh->GetIndices().size() );
                // Index streams are immutable and fingerprinted at import. Hashing
                // their bytes here used to rescan each dragon for every instance,
                // every view, before even reaching the unchanged-scene cache.
                Hash( inputs.mTopologyHash, item.mMesh->GetTopologyHash() );
            }
            inputs.mMeshes.push_back( eastl::move( item ) );
        }
    }

    auto RayTracingModule::PackMaterial( material::SurfaceMaterial &source, SceneInputs &inputs ) const -> Material {
        // Resolve streamed textures before building the material's descriptor indices.
        source.UpdateTextureResidency();
        const auto materialTextureIndex{ [&]( material::MapType type ) -> core::i32 {
            return source.IsTextureUsed( type ) ? inputs.AddTexture( source.GetTexture( type ) ) : kInvalidTextureIndex;
        } };

        Material result{};
        const auto shadingModel{ source.GetShadingModel() };
        const bool specularWorkflow{ source.GetWorkflow() == material::Workflow::eSpecularGlossiness &&
                                     shadingModel != material::ShadingModel::eConductor && shadingModel != material::ShadingModel::eUnlit };
        result.mBase = specularWorkflow ? source.GetDiffuseFactor() : source.GetBaseColorFactor();
        result.mEmission = core::float4{ source.GetEmissiveFactor() * source.GetEmissiveStrength(), 0.0f };
        result.mParameters = core::float4{ specularWorkflow ? 0.0f : source.GetMetallicFactor(),
            specularWorkflow ? 1.0f - source.GetGlossinessFactor() : source.GetRoughnessFactor(),
            static_cast<float>( source.GetAlphaMask() ), source.GetAlphaMaskCutoff() };
        result.mTextures = glm::ivec4{ materialTextureIndex( specularWorkflow ? material::MapType::eDiffuse : material::MapType::eBaseColor ),
            materialTextureIndex( specularWorkflow ? material::MapType::eSpecularGlossiness : material::MapType::eMetallicRoughness ),
            materialTextureIndex( material::MapType::eEmissive ),
            materialTextureIndex( material::MapType::eNormal ) };
        result.mExtra = core::float4{ source.GetNormalScale(), source.IsDoubleSided() ? 1.0f : 0.0f, 0.0f, specularWorkflow ? 1.0f : 0.0f };
        result.mUVSets = glm::ivec4{ source.GetBaseColorTextureSet(),
            specularWorkflow ? source.GetSpecularGlossinessSet() : source.GetMetallicRoughnessTextureSet(),
            source.GetEmissiveTextureSet(), source.GetNormalTextureSet() };
        result.mSpecular = source.GetSpecularFactor();
        result.mUvTransform = source.GetUvTransform();

        const auto &checker{ source.GetCheckerboard() };
        result.mCheckerboardColorA = { checker.mColorA, checker.mEnabled ? 1.0f : 0.0f };
        result.mCheckerboardColorB = { checker.mColorB, checker.mCellSize };
        result.mCheckerboardOffset = { checker.mOffset, 0.0f };

        const auto &height{ source.GetHeightMapping() };
        result.mHeightMapping = { height.mScale, height.mMidlevel, as<float>( height.mMode ), as<float>( height.mSteps ) };
        result.mHeightInfo = { materialTextureIndex( material::MapType::eHeight ), as<i32>( height.mUVSet ), 0, 0 };

        // Transmissive models belong to full path tracing. Opaque conductor/unlit
        // responses also apply to surfaces encountered by hybrid reflection rays.
        if ( mConfiguration.mPathTracingEnabled || shadingModel == material::ShadingModel::eConductor || shadingModel == material::ShadingModel::eUnlit ) {
            const auto model{ shadingModel };
            result.mProperties.x = as<u32>( model );
            if ( model == material::ShadingModel::eDielectric ) {
                const auto &optical{ source.GetDielectric() };
                result.mProperties.y = optical.mThinWalled;
                result.mOptical = float4{ optical.mIndexOfRefraction, optical.mTransmission, optical.mThickness, 0 };
                result.mAbsorption = float4{ -glm::log( optical.mAttenuationColor ) / optical.mAttenuationDistance, optical.mPhaseAnisotropy };
                result.mScattering = float4{ optical.mScattering, 0 };
            }
            else if ( model == material::ShadingModel::eSubsurfaceScattering ) {
                const auto &optical{ source.GetSubsurface() };
                result.mOptical = float4{ optical.mIndexOfRefraction, 1, 0, 0 };
                result.mAbsorption = float4{ optical.mAbsorption / optical.mUnitsPerMillimeter, optical.mAnisotropy };
                result.mScattering = float4{ optical.mScattering / optical.mUnitsPerMillimeter, 0 };
            }
            else if ( model == material::ShadingModel::eConductor ) {
                const auto &optical{ source.GetConductor() };
                result.mAbsorption = float4{ optical.mEta, 0 };
                result.mScattering = float4{ optical.mK, 0 };
            }
        }

        return result;
    }

    auto RayTracingModule::CollectLights( const scene::Scene &scene, SceneInputs &inputs ) const -> void {
        const auto &registry{ scene.GetRegistry() };

        for ( const auto [entity, tag, transform, component]:
            registry.view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>().each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }

            Light light{};
            const auto type{ component.GetActiveType() };
            const auto &world{ transform.GetWorldTransform() };
            light.mPositionType = core::float4{ core::float3{ world[3] }, static_cast<float>( type ) };

            if ( type == LightType::eAreaLight ) {
                // Explicit area lights belong only to path tracing, including their visible surfaces.
                RectangleEmitter shape{};

                const auto &area{ component.Get<AreaLight>() };

                if ( mConfiguration.mPathTracingEnabled && mMeshesVisible && area.GetShape() == AreaLightShape::eRectangle &&
                     BuildRectangleEmitter( area, world, shape ) ) {
                    HashBytes( inputs.mHash, shape.mCorners.data(), sizeof( shape.mCorners ) );
                    Hash( inputs.mHash, shape.mRadiance );
                    Hash( inputs.mHash, shape.mTwoSided );
                    inputs.mRectangleEmitters.push_back( shape );
                    if ( mDenoiserDataBuffer.mHandle || mReconstructionDataBuffer.mHandle ) {
                        // Rectangle vertices follow mesh vertices in the packed scene.
                        Hash( inputs.mTopologyHash, entity );
                    }
                }

                continue;
            }
            else if ( type == LightType::eDirectional ) {
                const auto &source{ component.Get<DirectionalLight>() };
                light.mDirectionRadius = core::float4{ source.GetWorldDirection( world ), 0.0f };
                light.mColorIntensity = core::float4{ source.GetColor(), source.GetIntensity() };
                light.mSpot.z = mConfiguration.mPathTracingEnabled ? glm::radians( source.GetAngularRadius() ) : 0.0f;
            }
            else if ( type == LightType::ePoint ) {
                const auto &source{ component.Get<PointLight>() };
                light.mDirectionRadius.w = source.GetRadius();
                light.mColorIntensity = core::float4{ source.GetColor(), source.GetIntensity() };
                light.mSpot.z = mConfiguration.mPathTracingEnabled ? source.GetSourceRadius() : 0.0f;
            }
            else if ( type == LightType::eSpot ) {
                const auto &source{ component.Get<SpotLight>() };
                light.mDirectionRadius = core::float4{ source.GetWorldDirection( world ), source.GetRadius() };
                light.mColorIntensity = core::float4{ source.GetColor(), source.GetIntensity() };
                light.mSpot = core::float4{ source.GetCutOff(), source.GetOuterCutOff(), 0.0f, 0.0f };
            }
            else {
                continue;
            }

            inputs.mLights.push_back( light );
            Hash( inputs.mHash, light );
        }
    }

    auto RayTracingModule::PackScene( SceneInputs &inputs ) -> GeometryData {
        GeometryData next{};
        next.mTextures = eastl::move( inputs.mTextures );

        eastl::vector<Material> materials{};
        PackSceneMeshes( next, inputs.mMeshes, materials );
        AppendRectangleEmitters( next, inputs, materials );

        const auto triangleCount{ as<u32>( next.mIndices.size() / kTriangleIndexCount ) };

        // Keep the TLAS bindable for sky-only scenes using a masked-out triangle.
        if ( next.mIndices.empty() ) {
            next.mVertices.resize( kTriangleIndexCount );
            next.mIndices = { 0, 1, 2 };
            next.mBuildPositions.resize( kTriangleIndexCount );
            next.mMeshes.push_back( FGAccelerationGeometry{
                .mIndexCount = kTriangleIndexCount,
                .mMask = 0 } );
            materials.resize( 1 );
        }

        const auto emitters{ CollectTriangleEmitters( next, materials, triangleCount ) };
        PackSceneData( next, inputs, materials, emitters );
        AppendLightDistribution( next, inputs.mLights, materials, emitters );

        // Build indices address the entire shared position buffer.
        for ( auto &geometry: next.mMeshes ) {
            geometry.mVertexCount = as<u32>( next.mVertices.size() );
        }

        return next;
    }

    auto RayTracingModule::PackSceneMeshes( GeometryData &next, eastl::span<const MeshInstance> instances,
        eastl::vector<Material> &materials ) -> void {
        ankerl::unordered_dense::map<const material::SurfaceMaterial *, u32> materialSlots{};
        mPackedMeshCache.resize( instances.size() );

        usize meshSlot{};

        for ( const auto &instance: instances ) {
            const auto &mesh{ *instance.mMesh };
            const core::u32 base{ static_cast<core::u32>( next.mVertices.size() ) };
            u32 materialIndex{ as<u32>( materials.size() ) };

            if ( instance.mMeshEmitter ) {
                // Instances may share a surface material while carrying different light radiance.
                materials.push_back( instance.mMaterial );
            }
            else {
                const auto [slot, added]{ materialSlots.try_emplace( instance.mMaterialSource, materialIndex ) };
                materialIndex = slot->second;

                if ( added ) {
                    materials.push_back( instance.mMaterial );
                }
            }

            // InstanceID identifies the mesh's first triangle in the shared shader index buffer.
            const auto firstIndex{ as<u32>( next.mIndices.size() ) };
            FGAccelerationGeometry geometry{
                .mFirstIndex = firstIndex,
                .mIndexCount = as<u32>( mesh.GetIndices().size() ),
                .mInstanceID = firstIndex / kTriangleIndexCount,
                .mRevision = instance.mShapeRevision
            };

            // The TLAS expects a row-major 3x4 transform; GLM stores columns.
            for ( core::u32 row{}; row < kInstanceTransformRows; ++row ) {
                for ( core::u32 column{}; column < kInstanceTransformColumns; ++column ) {
                    geometry.mTransform[row * kInstanceTransformColumns + column] = instance.mTransform[column][row];
                }
            }

            auto &cached{ mPackedMeshCache[meshSlot++] };
            u64 vertexHash{ instance.mShapeRevision };
            Hash( vertexHash, instance.mTransform );
            Hash( vertexHash, materialIndex );
            Hash( vertexHash, base );

            // Reuse transformed attributes for unchanged meshes when another character animates.
            // This mirrors the revision-based skinning cache in GeometryCullModule.
            if ( cached.mHash == vertexHash && !cached.mVertices.empty() ) {
                next.mVertices.insert( next.mVertices.end(), cached.mVertices.begin(), cached.mVertices.end() );
                next.mBuildPositions.insert( next.mBuildPositions.end(), cached.mBuildPositions.begin(), cached.mBuildPositions.end() );
                next.mIndices.insert( next.mIndices.end(), cached.mIndices.begin(), cached.mIndices.end() );
                next.mMeshes.push_back( geometry );
                continue;
            }

            PackMeshVertices( next, instance, base, materialIndex );

            const auto indices{ mesh.GetIndices() };
            const bool mirrored{ glm::determinant( glm::mat3{ instance.mTransform } ) < 0.0f };

            for ( core::usize i{}; i + kTriangleIndexCount - 1 < indices.size(); i += kTriangleIndexCount ) {
                next.mIndices.push_back( base + indices[i] );
                next.mIndices.push_back( base + indices[i + ( mirrored ? 2 : 1 )] );
                next.mIndices.push_back( base + indices[i + ( mirrored ? 1 : 2 )] );
            }

            cached.mHash = vertexHash;
            cached.mVertices.assign( next.mVertices.begin() + base, next.mVertices.end() );
            cached.mBuildPositions.assign( next.mBuildPositions.begin() + base, next.mBuildPositions.end() );
            cached.mIndices.assign( next.mIndices.begin() + geometry.mFirstIndex, next.mIndices.end() );
            next.mMeshes.push_back( geometry );
        }
    }

    auto RayTracingModule::PackMeshVertices( GeometryData &next, const MeshInstance &instance, u32 base, u32 materialIndex ) -> void {
        const auto &mesh{ *instance.mMesh };
        const auto bytes{ mesh.GetAttributes() };
        const auto &offsets{ mesh.GetOffsets() };
        const auto read{ [&]( MeshStream stream, u32 vertex, void *destination, usize size ) {
            std::memcpy( destination, bytes.data() + offsets[as<usize>( stream )] + vertex * size, size );
        } };

        const auto *source{ instance.mMaterialSource };
        const bool displaced{ source && source->GetHeightMapping().mMode == material::HeightMappingMode::eDisplacement &&
                              source->IsTextureUsed( material::MapType::eHeight ) && source->GetTexture( material::MapType::eHeight ) };
        const auto heightImage{ displaced ? source->GetHeightImage() : asset::ImageHandle{} };

        // The BLAS uses local positions. Shading and light sampling use world-space attributes.
        const auto normalMatrix{ glm::inverseTranspose( glm::mat3{ instance.mTransform } ) };
        const auto inverseTransform{ glm::inverse( instance.mTransform ) };
        float3 localMinimum{ std::numeric_limits<f32>::max() };
        float3 localMaximum{ std::numeric_limits<f32>::lowest() };

        for ( core::u32 i{}; i < mesh.GetVertexCount(); ++i ) {
            core::float3 position{};
            core::float3 normal{};
            Vertex vertex{};
            read( MeshStream::Position, i, &position, kPositionStreamStride );
            read( MeshStream::Normal, i, &normal, kPositionStreamStride );
            read( MeshStream::Color, i, &vertex.mColor, sizeof( vertex.mColor ) );
            read( MeshStream::UV0, i, &vertex.mUV.x, sizeof( float2 ) );
            read( MeshStream::UV1, i, &vertex.mUV.z, sizeof( float2 ) );

            // Apply local displacement before skinning so traversal sees the posed surface.
            if ( heightImage ) {
                const auto &mapping{ source->GetHeightMapping() };
                const auto transform{ source->GetUvTransform() };
                const auto uv{ ( mapping.mUVSet == 0 ? float2{ vertex.mUV.x, vertex.mUV.y } : float2{ vertex.mUV.z, vertex.mUV.w } ) *
                                   float2{ transform.x, transform.y } +
                               float2{ transform.z, transform.w } };

                if ( glm::dot( normal, normal ) > 1e-12f ) {
                    position += glm::normalize( normal ) * ( SampleDisplacementHeight( *heightImage, uv ) - mapping.mMidlevel ) * mapping.mScale;
                }
            }

            if ( !instance.mPalette.empty() ) {
                core::float4x4 skin{ 0.0f };
                f32 total{};

                for ( core::u32 group{}; group < 2; ++group ) {
                    core::float4 joints{};
                    core::float4 weights{};
                    read( group ? MeshStream::Joints1 : MeshStream::Joints0, i, &joints, sizeof( joints ) );
                    read( group ? MeshStream::Weights1 : MeshStream::Weights0, i, &weights, sizeof( weights ) );

                    for ( core::u32 component{}; component < 4; ++component ) {
                        if ( weights[component] > 0.0f &&
                             joints[component] >= 0.0f && joints[component] < instance.mPalette.size() ) {
                            skin += instance.mPalette[static_cast<core::u32>( joints[component] )] * weights[component];
                            total += weights[component];
                        }
                    }
                }

                if ( total > 0.0f ) {
                    skin /= total;
                    position = core::float3{ skin * core::float4{ position, 1.0f } };
                    normal = glm::mat3{ skin } * normal;
                }
            }

            auto localPosition{ position };
            position = core::float3{ instance.mTransform * core::float4{ position, 1.0f } };
            normal = normalMatrix * normal;

            if ( instance.mFoliageMotion.x > 0.0f ) {
                const auto rest{ position };
                normal = glm::length( normal ) > 0.000001f ? glm::normalize( normal ) : float3{ 0, 1, 0 };
                position = DeformFoliagePosition( rest, instance );
                normal = DeformFoliageNormal( rest, position, normal, instance );
                // TLAS retains the instance transform, so the BLAS stores the
                // inverse-transformed wind pose rather than applying placement twice.
                localPosition = float3{ inverseTransform * float4{ position, 1.0f } };
            }

            next.mBuildPositions.push_back( localPosition );
            localMinimum = glm::min( localMinimum, localPosition );
            localMaximum = glm::max( localMaximum, localPosition );
            vertex.mPosition = core::float4{ position, static_cast<float>( materialIndex ) };
            vertex.mNormal = core::float4{ glm::length( normal ) > 0.000001f ? glm::normalize( normal ) : core::float3{ 0, 1, 0 }, 0.0f };
            next.mVertices.push_back( vertex );
        }

        // Bound connections to sampled emitters without fetching instance matrices in raygen.
        const auto linear{ AbsoluteLinear( instance.mTransform ) };
        const auto inverseLinear{ AbsoluteLinear( inverseTransform ) };
        const auto translation{ glm::abs( float3{ instance.mTransform[3] } ) };
        const auto inverseTranslation{ glm::abs( float3{ inverseTransform[3] } ) };
        const auto extents{ 2.0f * ( localMaximum - localMinimum ) };
        const f32 extent{ glm::max( extents.x, glm::max( extents.y, extents.z ) ) };

        for ( u32 index{ base }; index < next.mVertices.size(); ++index ) {
            auto &vertex{ next.mVertices[index] };
            vertex.mNormal.w = SampledPositionError( next.mBuildPositions[index], float3{ vertex.mPosition }, extent,
                linear, translation, inverseLinear, inverseTranslation );
        }
    }

    auto RayTracingModule::AppendRectangleEmitters( GeometryData &next, const SceneInputs &inputs,
        eastl::vector<Material> &materials ) -> void {
        // Bake rectangular lights into the path-only BLAS so camera, reflection and shadow
        // rays see the same surface sampled by next-event estimation. Radiance has no range cutoff.
        for ( const auto &shape: inputs.mRectangleEmitters ) {
            Material material{};
            material.mBase = float4{ 0, 0, 0, 1 };
            material.mEmission = float4{ shape.mRadiance, shape.mTwoSided ? 2.0f : 1.0f };
            material.mParameters.y = 1.0f;
            material.mExtra.y = shape.mTwoSided ? 1.0f : 0.0f;
            material.mExtra.z = 1.0f;// Pure emitter; sided emission is independent of opaque visibility.
            const u32 materialIndex{ as<u32>( materials.size() ) };
            materials.push_back( material );

            const auto firstIndex{ as<u32>( next.mIndices.size() ) };
            const FGAccelerationGeometry geometry{
                .mFirstIndex = firstIndex,
                .mIndexCount = 2 * kTriangleIndexCount,
                .mInstanceID = firstIndex / kTriangleIndexCount
            };
            const u32 base{ as<u32>( next.mVertices.size() ) };
            const auto extents{ 2.0f * ( glm::abs( shape.mCorners[1] - shape.mCorners[0] ) +
                                           glm::abs( shape.mCorners[3] - shape.mCorners[0] ) ) };
            const f32 extent{ glm::max( extents.x, glm::max( extents.y, extents.z ) ) };

            for ( const auto &position: shape.mCorners ) {
                Vertex vertex{};
                vertex.mPosition = float4{ position, as<f32>( materialIndex ) };
                vertex.mNormal = float4{ shape.mNormal, SampledPositionError( position, position, extent,
                                                            glm::mat3{ 1 }, float3{ 0 }, glm::mat3{ 1 }, float3{ 0 } ) };
                vertex.mColor = float4{ 1 };
                next.mVertices.push_back( vertex );
                next.mBuildPositions.push_back( position );
            }

            for ( const u32 index: { 0u, 1u, 2u, 0u, 2u, 3u } ) {
                next.mIndices.push_back( base + index );
            }

            next.mMeshes.push_back( geometry );
        }
    }

    auto RayTracingModule::CollectTriangleEmitters( const GeometryData &next, eastl::span<const Material> materials,
        u32 triangleCount ) -> eastl::vector<Emitter> {
        // Weight emissive triangles by area and luminance, then normalize the selection CDF.
        eastl::vector<Emitter> emitters{};
        f32 totalWeight{};

        for ( core::u32 triangle{}; triangle < triangleCount; ++triangle ) {
            const auto &a{ next.mVertices[next.mIndices[triangle * kTriangleIndexCount]] };
            const auto &b{ next.mVertices[next.mIndices[triangle * kTriangleIndexCount + 1]] };
            const auto &c{ next.mVertices[next.mIndices[triangle * kTriangleIndexCount + 2]] };
            const auto emission{ core::float3{ materials[static_cast<core::u32>( a.mPosition.w )].mEmission } };
            const f32 area{ 0.5f * glm::length( glm::cross( core::float3{ b.mPosition - a.mPosition }, core::float3{ c.mPosition - a.mPosition } ) ) };
            const f32 weight{ area * glm::dot( emission, core::float3{ 0.2126f, 0.7152f, 0.0722f } ) };

            if ( weight > 0.0f ) {
                totalWeight += weight;
                emitters.push_back( Emitter{
                    .mTriangle = triangle,
                    .mCDF = totalWeight,
                    .mArea = area,
                    .mWeight = weight } );
            }
        }

        for ( auto &emitter: emitters ) {
            emitter.mCDF /= totalWeight;
            emitter.mWeight /= totalWeight;
        }

        return emitters;
    }

    auto RayTracingModule::PackSceneData( GeometryData &next, const SceneInputs &inputs, eastl::span<Material> materials,
        eastl::span<const Emitter> emitters ) -> void {
        const auto lights{ eastl::span<const Light>{ inputs.mLights } };
        const auto lightOffset{ sizeof( SceneHeader ) + materials.size_bytes() };
        const auto emitterOffset{ lightOffset + lights.size_bytes() };

        // Profiles belong to the material upload, so camera motion never rebuilds diffusion tables.
        eastl::vector<material::BssrdfProfile> profiles{};
        const auto profileBase{ emitterOffset + emitters.size() * sizeof( Emitter ) };
        for ( auto &record: materials ) {
            if ( record.mProperties.x != as<u32>( material::ShadingModel::eSubsurfaceScattering ) ) {
                continue;
            }
            material::SubsurfaceSettings settings{};
            settings.mIndexOfRefraction = record.mOptical.x;
            settings.mAnisotropy = record.mAbsorption.w;
            settings.mAbsorption = float3{ record.mAbsorption };
            settings.mScattering = float3{ record.mScattering };
            record.mProperties.z = as<u32>( profileBase + profiles.size() * sizeof( material::BssrdfProfile ) );
            profiles.push_back( material::BuildBssrdfProfile( settings ) );
        }

        // Matches SceneRecord and the byte offsets consumed by the Slang shaders.
        const SceneHeader header{
            .mMaterialCount = as<u32>( materials.size() ),
            .mLightCount = as<u32>( lights.size() ),
            .mEmitterCount = as<u32>( emitters.size() ),
            .mLightOffset = as<u32>( lightOffset ),
            .mEmitterOffset = as<u32>( emitterOffset ),
            .mEnvironmentIndex = inputs.mEnvironmentIndex,
            .mFaceOffset = inputs.mEnvironmentFaceBase,
            .mHasFaces = as<u32>( inputs.mCubeEnvironment )
        };

        AppendSceneBytes( next.mSceneData, &header, sizeof( header ) );
        AppendSceneBytes( next.mSceneData, materials.data(), materials.size_bytes() );
        AppendSceneBytes( next.mSceneData, lights.data(), lights.size_bytes() );
        AppendSceneBytes( next.mSceneData, emitters.data(), emitters.size_bytes() );
        AppendSceneBytes( next.mSceneData, profiles.data(), profiles.size() * sizeof( material::BssrdfProfile ) );
    }

    auto RayTracingModule::AppendLightDistribution( GeometryData &next, eastl::span<const Light> lights,
        eastl::span<const Material> materials, eastl::span<const Emitter> emitters ) const -> void {
        // PBRT PowerLightSampler: use emitted-power estimates to select a light,
        // and retain the exact PMF for both next-event estimation and emitter-hit MIS.
        // https://pbr-book.org/4ed/Light_Sources/Light_Sampling#PowerLightSampler
        float3 minimum{ std::numeric_limits<f32>::max() };
        float3 maximum{ std::numeric_limits<f32>::lowest() };

        for ( const auto &vertex: next.mVertices ) {
            minimum = glm::min( minimum, float3{ vertex.mPosition } );
            maximum = glm::max( maximum, float3{ vertex.mPosition } );
        }

        constexpr f64 pi{ 3.14159265358979323846 };
        const f64 radius2{ glm::max( glm::dot( maximum - minimum, maximum - minimum ) * .25f, .0001f ) };
        const auto luminance{ []( const float3 &color ) -> f64 {
            return glm::dot( glm::max( color, float3{ 0 } ), float3{ .2126f, .7152f, .0722f } );
        } };
        eastl::vector<f64> powers{};
        for ( const auto &light: lights ) {
            float3 illumination{ float3{ light.mColorIntensity } * light.mColorIntensity.w };
            const bool directional{ light.mPositionType.w == as<f32>( LightType::eDirectional ) };
            const bool spotlight{ light.mPositionType.w == as<f32>( LightType::eSpot ) };

            if ( directional && mViewEffects.mControl.x != 0 &&
                 glm::dot( -glm::normalize( float3{ light.mDirectionRadius } ), float3{ mViewEffects.mAtmosphere.mSunDirection } ) > .9999f ) {
                // Match the explicit atmospheric sun used by the ray shader's light sample.
                illumination = float3{ mViewEffects.mAtmosphere.mSunIlluminance };
            }

            const f64 intensity{ luminance( illumination ) };
            const f64 solidAngle{ directional ? pi * radius2
                                  : spotlight ? 2 * pi * ( 1 - .5 * ( light.mSpot.x + light.mSpot.y ) )
                                              : 4 * pi };
            powers.push_back( glm::max( intensity * solidAngle, 0.0 ) );
        }

        if ( !emitters.empty() ) {
            f64 power{};
            for ( const auto &emitter: emitters ) {
                const auto &vertex{ next.mVertices[next.mIndices[emitter.mTriangle * kTriangleIndexCount]] };
                const auto &material{ materials[as<u32>( vertex.mPosition.w )] };
                const bool twoSided{ material.mEmission.w > 1 || ( material.mEmission.w == 0 && material.mExtra.y != 0 ) };
                power += pi * emitter.mArea * luminance( float3{ material.mEmission } ) * ( twoSided ? 2 : 1 );
            }
            powers.push_back( power );
        }

        // Keep the sky slot even when disabled. The shader conditions the distribution
        // on the active light range; a unit-radiance sky is a proposal estimate only.
        powers.push_back( 4 * pi * pi * radius2 );

        // A uniform component gives every active light a nonzero selection probability.
        constexpr f64 uniformWeight{ 0.05 };
        constexpr f64 powerWeight{ 0.95 };

        f64 total{};
        for ( const auto power: powers ) {
            total += power;
        }

        eastl::vector<float2> distribution{};
        f64 cumulative{};
        f32 previousCDF{};

        for ( usize i{}; i < powers.size(); ++i ) {
            cumulative += powerWeight * powers[i] / total + uniformWeight / powers.size();
            const f32 nextCDF{ i + 1 == powers.size() ? 1.f : as<f32>( cumulative ) };
            distribution.emplace_back( nextCDF, nextCDF - previousCDF );
            previousCDF = nextCDF;
        }

        next.mLightDistribution = as<u32>( next.mSceneData.size() );
        AppendSceneBytes( next.mSceneData, distribution.data(), distribution.size() * sizeof( float2 ) );
    }

}// namespace mikoto::renderer
