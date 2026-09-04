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

#ifndef MIKOTO_MODEL_HH
#define MIKOTO_MODEL_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/SkinnedAnimation.hh>

#include <Filesystem/File.hh>

#include <Material/PhysicalMaterial.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    using AnimationList = ankerl::unordered_dense::map<eastl::string, eastl::unique_ptr<animation::SkinnedAnimation>>;

    // These 2 are vec4 because they need to match the bone influence
    // which is maximum bones influence a vertex ( from what we support now )
    inline constexpr core::u32 kMaxBoneInfluence{ 4 };
    inline constexpr core::u32 kMaxBonesPerMesh{ 256 }; // Needs to match shader's

    enum class VertexAttribute {
        ePositions,
        eNormals,
        eTangents,
        eBitangents,
        eUV0,
        eUV1,
        eColors,
        eJoints,
        eWeights,
    };

    struct ModelLoadDescription {
        filesystem::FileHandle mFile{};
        renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eInvalid };
        bool mExtractTextures{ true };

        auto LoadTextures( bool value ) -> ModelLoadDescription&;
        auto WithFilePath( FileHandle file ) -> ModelLoadDescription&;
    };

    struct MeshCreateDescription {
        renderer::rhi::BufferHandle mVertices{};
        renderer::rhi::BufferHandle mIndices{};

        core::float4x4 mTransform{};
        eastl::string_view mName{};
        material::PhysicMaterialDescription mProperties{};

        auto SetName( eastl::string_view name ) -> MeshCreateDescription&;
        auto SetTransform( const core::float4x4& t ) -> MeshCreateDescription&;
        auto SetMaterial( const material::PhysicMaterialDescription& mat ) -> MeshCreateDescription&;
        auto SetVertices( renderer::rhi::BufferHandle vertices ) -> MeshCreateDescription&;
        auto SetIndices( renderer::rhi::BufferHandle indices ) -> MeshCreateDescription&;
    };

    class MeshNode final {
    public:
        MeshNode(core::u32 index, const MeshCreateDescription& desc);
        MeshNode(asset::MeshNode&& other) noexcept = default;

        MKT_NODISCARD auto GetName() -> const eastl::string& { return mName; }

        MKT_NODISCARD auto GetTransform() const -> const float4x4& { return mTransform; }
        MKT_NODISCARD auto GetMeshIndex() const -> size_t { return mMeshIndex; }
        MKT_NODISCARD auto GetVertexBuffer() -> renderer::rhi::BufferHandle { return mVertices; }
        MKT_NODISCARD auto GetIndexBuffer() -> renderer::rhi::BufferHandle { return mIndices; }

        MKT_NODISCARD auto GetVertexBuffer() const -> renderer::rhi::BufferHandle { return mVertices; }
        MKT_NODISCARD auto GetIndexBuffer() const -> renderer::rhi::BufferHandle { return mIndices; }

        MKT_NODISCARD auto GetProperties() const -> const material::PhysicMaterialDescription& { return mProperties; }

        DISABLE_COPY_FOR(MeshNode);

    private:
        eastl::string mName{};
        size_t mMeshIndex{};

        renderer::rhi::BufferHandle mIndices{};
        renderer::rhi::BufferHandle mVertices{};

        material::PhysicMaterialDescription mProperties{};

        float4x4 mTransform{ 1.0f }; // Identity by default
    };

    struct ModelCreateDescription {
        Path mPath{};
        eastl::string mName{};

        AnimationList mAnimations{};
        eastl::unique_ptr<animation::Skeleton> mSkeleton{};
        ankerl::unordered_dense::map<u32, MeshNode> mMeshes{};

        auto SetPath(const Path& path) -> ModelCreateDescription&;
        auto AddMesh(u32 index, const MeshCreateDescription& desc) -> ModelCreateDescription&;
        auto SetName(eastl::string_view name) -> ModelCreateDescription&;
        auto SetSkeleton(eastl::unique_ptr<animation::Skeleton>&& skeleton) -> ModelCreateDescription&;
        auto SetAnimations(AnimationList&& animations) -> ModelCreateDescription&;
    };

    class Model final : public ReferenceCounted {
    public:
        explicit Model( ModelCreateDescription&& desc )
            : mPath{ eastl::move( desc.mPath ) },
              mName{ eastl::move( desc.mName ) },
              mSkeleton { eastl::move(desc.mSkeleton) },
              mAnimations{ eastl::move( desc.mAnimations ) },
              mMeshes{ eastl::move( desc.mMeshes ) }
        {}

        MKT_NODISCARD auto GetMeshNodeCount() const -> size_t;

        MKT_NODISCARD auto GetMeshNode( size_t index) -> MeshNode&;
        MKT_NODISCARD auto GetMeshNode( size_t index) const -> const MeshNode&;

        MKT_NODISCARD auto GetPath() const -> const Path&;
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto IsSkinned() const -> bool;
        MKT_NODISCARD auto HasArmature() const -> bool;
        MKT_NODISCARD auto HasAnimations() const -> bool;

        MKT_NODISCARD auto GetSkeleton() const -> const animation::Skeleton*;

        MKT_NODISCARD auto FindAnimation( eastl::string_view name ) -> animation::SkinnedAnimation*;
        MKT_NODISCARD auto FindAnimation( eastl::string_view name ) const -> const animation::SkinnedAnimation*;

        MKT_NODISCARD auto GetAnimations() -> AnimationList&;
        MKT_NODISCARD auto GetAnimations() const -> const AnimationList&;

        ~Model() override = default;

    private:
        DISABLE_COPY_AND_MOVE_FOR( Model );

    private:
        // Only the factory can construct models
        friend class MeshFactory;

    protected:
        Path mPath{};
        eastl::string mName{};

        // Skinning
        AnimationList mAnimations{};
        eastl::unique_ptr<animation::Skeleton> mSkeleton{};

        // ( Mesh index, mesh node )
        ankerl::unordered_dense::map<u32, MeshNode> mMeshes{};
    };

    using ModelHandle = Ref<Model>;

}

#endif// MIKOTO_MODEL_HH