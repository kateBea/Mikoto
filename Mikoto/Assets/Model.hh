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
#include <EASTL/string_view.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/SkinnedAnimation.hh>

#include <Filesystem/File.hh>

#include <Material/PhysicalMaterial.hh>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::material;
    using namespace mikoto::animation;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    using AnimationList = ankerl::unordered_dense::map<eastl::string, animation::SkinnedAnimation>;

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
        FileHandle mFile{};
        GraphicsAPI mApi{ GraphicsAPI::eInvalid };
        bool mExtractTextures{ true };

        // Specifies the order we want attributes in
        // Default is order specified by DEFAULT_VERTEX_BUFFER_LAYOUT in Pipeline.hh file
        eastl::vector<VertexAttribute> mAttributes{
            VertexAttribute::ePositions,
            VertexAttribute::eNormals,
            VertexAttribute::eColors,
            VertexAttribute::eUV0,
            VertexAttribute::eUV1,

            VertexAttribute::eJoints, // Bone IDs
            VertexAttribute::eWeights, // Weight IDs
        };

        auto LoadTextures( bool value ) -> ModelLoadDescription&;
        auto WithFilePath( FileHandle file ) -> ModelLoadDescription&;
    };

    struct MeshCreateDescription {
        BufferHandle mVertices{};
        BufferHandle mIndices{};

        float4x4 mTransform{};
        eastl::string_view mName{};
        MaterialProperties mProperties{};

        auto SetName( eastl::string_view name ) -> MeshCreateDescription&;
        auto SetTransform( const float4x4& t ) -> MeshCreateDescription&;
        auto SetMaterial( const MaterialProperties& mat ) -> MeshCreateDescription&;
        auto SetVertices( BufferHandle vertices ) -> MeshCreateDescription&;
        auto SetIndices( BufferHandle indices ) -> MeshCreateDescription&;
    };

    class MeshNode final {
    public:
        MeshNode( u32 index, const MeshCreateDescription& desc);
        MeshNode(MeshNode&& other) noexcept = default;

        MKT_NODISCARD auto GetName() -> const eastl::string& { return mName; }

        MKT_NODISCARD auto GetTransform() const -> const float4x4& { return mTransform; }
        MKT_NODISCARD auto GetMeshIndex() const -> size_t { return mMeshIndex; }
        MKT_NODISCARD auto GetVertexBuffer() -> BufferHandle { return  mVertices; }
        MKT_NODISCARD auto GetIndexBuffer() -> BufferHandle { return mIndices; }

        MKT_NODISCARD auto GetVertexBuffer() const -> BufferHandle { return mVertices; }
        MKT_NODISCARD auto GetIndexBuffer() const -> BufferHandle { return mIndices; }

        MKT_NODISCARD auto GetProperties() const -> const MaterialProperties& { return mProperties; }

        DISABLE_COPY_FOR( MeshNode );

    private:
        eastl::string mName{};
        size_t mMeshIndex{};

        BufferHandle mIndices{};
        BufferHandle mVertices{};

        MaterialProperties mProperties{};

        float4x4 mTransform{ 1.0f }; // Identity by default
    };

    struct ModelCreateDescription {
        Path mPath{};
        eastl::string mName{};

        Skeleton mSceneSkeleton{};
        AnimationList mAnimations{};
        ankerl::unordered_dense::map<u32, MeshNode> mMeshes{};

        auto SetPath(const Path& path) -> ModelCreateDescription&;
        auto AddMesh(u32 index, const MeshCreateDescription& desc) -> ModelCreateDescription&;
        auto SetName(eastl::string_view name) -> ModelCreateDescription&;
        auto SetSkeleton(Skeleton&& skeleton) -> ModelCreateDescription&;
        auto SetAnimations(AnimationList&& animations) -> ModelCreateDescription&;
    };

    class Model final : public ReferenceCounted {
    public:
        explicit Model( ModelCreateDescription&& desc )
            : mPath{ eastl::move( desc.mPath ) },
              mName{ eastl::move( desc.mName ) },
              mSkeleton { eastl::move(desc.mSceneSkeleton) },
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

        MKT_NODISCARD auto GetSkeleton() const -> const Skeleton&;

        MKT_NODISCARD auto FindAnimation( eastl::string_view name ) -> SkinnedAnimation*;
        MKT_NODISCARD auto FindAnimation( eastl::string_view name ) const -> const SkinnedAnimation*;

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
        Skeleton mSkeleton{};
        AnimationList mAnimations{};

        // ( Mesh index, mesh node )
        ankerl::unordered_dense::map<u32, MeshNode> mMeshes{};
    };

    using ModelHandle = Ref<Model>;

}

#endif// MIKOTO_MODEL_HH