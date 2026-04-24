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

#ifndef MIKOTO_IMPORTER_HH
#define MIKOTO_IMPORTER_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Assets/Model.hh>

#include <Renderer/Core/GpuDevice.hh>

#include <Memory/BufferSpan.hh>

#include <Animation/Animator.hh>
#include <Animation/SkinnedAnimation.hh>

namespace mikoto::asset {

    struct VertexDescription {
        float3 mPosition{};
        float3 mNormals{};
        float4 mColors{ 1.0f, 1.0f, 1.0f, 1.0f };

        float2 mUv0{};
        float2 mUv1{};

        float4 mJoints0{ -1.0f, -1.0f, -1.0f, -1.0f };
        float4 mWeights0{};

        float4 mJoints1{ -1.0f, -1.0f, -1.0f, -1.0f };
        float4 mWeights1{};
    };

    struct MeshNodeDescription {
        eastl::string mName{};

        float4x4 mTransform{ 1.0f }; // Identity by default

        eastl::vector<u32> mIndices{};
        eastl::vector<VertexDescription> mVertices{};

        // Unsigned because it needs at least one material
        u32 MaterialIndex{};
    };

    struct ModelDataDescription {
        eastl::string mName{};

        eastl::vector<MeshNodeDescription> mMeshNodes{};
        eastl::vector<MaterialProperties> mMaterials{};

        Skeleton mSceneSkeleton{};
        AnimationList mAnimations{};

        // Texture URI the same way is stored in the materials
        ankerl::unordered_dense::map<Path, TextureHandle> mTextures{};
    };

    class ModelImporter {
    public:
        explicit ModelImporter(GpuDevice* device)
            : mDevice{ device } {}

        virtual auto Import(const ModelLoadDescription& description, ModelDataDescription& out) -> void = 0;

        virtual ~ModelImporter() = default;

    protected:
        GpuDevice* mDevice{ nullptr };
    };

}

#endif//MIKOTO_IMPORTER_HH
