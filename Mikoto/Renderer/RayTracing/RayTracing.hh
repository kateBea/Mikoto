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

#ifndef MIKOTO_RENDERER_RAY_TRACING_RAY_TRACING_HH
#define MIKOTO_RENDERER_RAY_TRACING_RAY_TRACING_HH

#include <ankerl/unordered_dense.h>

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/SurfaceMaterial.hh>

#include <Renderer/Core/MeshRenderData.hh>
#include <Renderer/Rhi/Texture.hh>

namespace mikoto::renderer {

    // Transport options shared by the hybrid renderer and full path tracer.
    struct RayTracingConfiguration {
        static constexpr core::u32 kMaxPathSamplesPerFrame{ 1024 };

        bool mPathTracingEnabled{};

        bool mShadowsEnabled{};
        bool mReflectionsEnabled{};
        bool mGlobalIlluminationEnabled{};
        bool mAmbientOcclusionEnabled{};

        bool mUseCosineWeightedAoSampling{ true };
        core::u32 mAoSamplesPerFrame{ 2 };
        core::f32 mAoMaxDistance{};// Zero selects PBRT's unlimited-distance default.

        core::u32 mShadowSamplesPerFrame{ 1 };
        core::u32 mReflectionMaxBounces{ 4 };

        // Shared indirect sampling budget for hybrid reflections and diffuse GI.
        core::u32 mRayGiSamplesPerFrame{ 1 };
        core::u32 mRayGiMaxBounces{ 2 };

        core::u32 mPathSamplesPerFrame{ 2 };
        core::u32 mPathMaxBounces{ 8 };
        core::u32 mPathMaxSamples{ 4096 };

        auto Sanitize() -> void;
    };

    inline constexpr core::u64 kRevisionHashSeed{ 1469598103934665603ull };
    inline constexpr core::u64 kRevisionHashPrime{ 1099511628211ull };

    inline constexpr core::u32 kTriangleIndexCount{ 3 };
    inline constexpr core::usize kPositionStreamStride{ 3 * sizeof( core::f32 ) };// Packed by MeshRenderData.
    inline constexpr core::u32 kInstanceTransformRows{ 3 };
    inline constexpr core::u32 kInstanceTransformColumns{ 4 };
    inline constexpr core::i32 kInvalidTextureIndex{ -1 };
    inline constexpr core::u32 kRayHitPayloadBytes{ 52 };// Hit in PathTracing_Common.slang.
    inline constexpr core::u32 kOcclusionHistoryLimit{ 4096 };
    inline constexpr core::u32 kHybridHistoryLimit{ 256 };

    // GPU records mirror the declarations in PathTracing_Common.slang.
    struct Material {
        core::float4 mBase{};
        core::float4 mEmission{};
        core::float4 mParameters{};
        glm::ivec4 mTextures{ kInvalidTextureIndex };
        core::float4 mExtra{};
        glm::ivec4 mUVSets{};
        core::float4 mSpecular{};
        core::float4 mUvTransform{ 1, 1, 0, 0 };
        glm::uvec4 mProperties{};  // Shading model, thin wall, BSSRDF byte offset, reserved.
        core::float4 mOptical{};   // IOR, transmission, artist thickness, reserved.
        core::float4 mAbsorption{};// Inverse world units, phase anisotropy.
        core::float4 mScattering{};// Inverse world units, reserved.
        core::float4 mHeightMapping{};
        glm::ivec4 mHeightInfo{ kInvalidTextureIndex, 0, 0, 0 };
        core::float4 mCheckerboardColorA{};// RGB tint, enabled in W.
        core::float4 mCheckerboardColorB{};// RGB tint, world-space cell size in W.
        core::float4 mCheckerboardOffset{};
    };

    struct Light {
        core::float4 mPositionType{};
        core::float4 mDirectionRadius{};
        core::float4 mColorIntensity{};
        core::float4 mSpot{};// Cone cosines (xy), PT point radius / directional half-angle in radians (z).
    };

    struct RectangleEmitter {
        eastl::array<core::float3, 4> mCorners{};
        core::float3 mNormal{};
        core::float3 mRadiance{};
        bool mTwoSided{};
    };

    // CPU snapshot used to detect scene changes and pack the GPU scene records.
    struct MeshInstance {
        Material mMaterial{};
        core::u64 mShapeRevision{};
        MeshRenderDataHandle mMesh{};
        core::float4x4 mTransform{ 1.0f };
        eastl::vector<core::float4x4> mPalette{};
        core::float4 mFoliageMotion{};
        core::float4 mFoliageShape{};
        core::float4 mFoliageWind{};
        const material::SurfaceMaterial *mMaterialSource{};

        bool mMeshEmitter{};
    };

    struct SceneInputs {
        eastl::vector<Light> mLights{};
        eastl::vector<MeshInstance> mMeshes{};
        eastl::vector<rhi::TextureHandle> mTextures{};
        eastl::vector<RectangleEmitter> mRectangleEmitters{};

        ankerl::unordered_dense::map<const rhi::ITexture *, core::i32> mTextureIndices{};

        core::i32 mEnvironmentIndex{ kInvalidTextureIndex };
        core::u32 mEnvironmentFaceBase{};
        core::u64 mHash{ kRevisionHashSeed };
        core::u64 mTopologyHash{ kRevisionHashSeed };
        core::u64 mLightingHash{ kRevisionHashSeed };

        bool mCubeEnvironment{};

        // Material references use these local indices until the frame resolves descriptors.
        MKT_NODISCARD auto AddTexture( const rhi::TextureHandle &texture ) -> core::i32 {
            if ( !texture || texture->GetDimension() != rhi::TextureDimension::eTexture2D ) {
                return kInvalidTextureIndex;
            }

            const auto [slot, added]{ mTextureIndices.try_emplace( texture.GetPtr(), core::as<core::i32>( mTextures.size() ) ) };

            if ( added ) {
                mTextures.push_back( texture );
            }

            return slot->second;
        }
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_RAY_TRACING_RAY_TRACING_HH
