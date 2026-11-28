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

#ifndef MIKOTO_ASSETS_IMPORTER_HH
#define MIKOTO_ASSETS_IMPORTER_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <Memory/BufferSpan.hh>

#include <Animation/SkinningPalette.hh>
#include <Animation/SkinnedAnimation.hh>

#include <Renderer/Core/Light.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    struct ModelLoadDescription;
    struct ModelDataDescription;

    // Aligned to make it easy to use in shaders
    struct alignas( 16 ) VertexDescription_Std430Alignment {
        core::float3 mPosition{};
        // meshoptimizer compares complete vertex bytes while reindexing, so
        // the explicit std430 padding must always have a deterministic value.
        core::f32 mPad0{};

        core::float3 mNormals{};
        core::f32 mPad1{};

        core::float4 mColors{ 1.0f, 1.0f, 1.0f, 1.0f };

        core::float2 mUv0{};
        core::float2 mUv1{};

        core::float4 mJoints0{ -1.0f, -1.0f, -1.0f, -1.0f };
        core::float4 mWeights0{};

        core::float4 mJoints1{ -1.0f, -1.0f, -1.0f, -1.0f };
        core::float4 mWeights1{};
    };

    struct VertexDescription {
        core::float3 mPosition{};

        core::float3 mNormals{};

        core::float4 mColors{ 1.0f, 1.0f, 1.0f, 1.0f };

        core::float2 mUv0{};
        core::float2 mUv1{};

        core::float4 mJoints0{ -1.0f, -1.0f, -1.0f, -1.0f };
        core::float4 mWeights0{};

        core::float4 mJoints1{ -1.0f, -1.0f, -1.0f, -1.0f };
        core::float4 mWeights1{};
    };

    struct MeshNodeDescription {
        eastl::string mName{};
        core::i32 mSourceNodeIndex{ -1 };
        // Aliases keep their own transform/skin; only this canonical entry owns vertex/index data.
        core::i32 mGeometrySourceIndex{ -1 };
        animation::MeshSkinBinding mSkinBinding{};

        core::float4x4 mTransform{ 1.0f };// Identity by default

        eastl::vector<core::u32> mIndices{};
        eastl::vector<VertexDescription_Std430Alignment> mVertices{};

        // Unsigned because it needs at least one material
        core::u32 MaterialIndex{};

        // KHR_lights_punctual
        bool mIsPunctualLight{ false };

        core::f32 mLightIntensity{ 1.0f };
        core::float3 mLightColor{ 1.0f, 1.0f, 1.0f };
        renderer::LightType mLightType{ renderer::LightType::eDirectional };

        // For spot lights we get two aditional parameters
        core::f32 mInnerConeAngle{};
        core::f32 mOuterConeAngle{};
    };

    class ModelImporter {
    public:
        explicit ModelImporter( renderer::rhi::IGpuDevice* device );

        virtual auto Import( const ModelLoadDescription& description, ModelDataDescription& out ) -> void = 0;

        virtual ~ModelImporter() = default;

    protected:
        renderer::rhi::IGpuDevice* mDevice{ nullptr };
    };

}// namespace mikoto::asset

#endif// MIKOTO_ASSETS_IMPORTER_HH
