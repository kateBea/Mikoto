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

#ifndef MIKOTO_MESH_CULLING_HH
#define MIKOTO_MESH_CULLING_HH

#include <EASTL/vector.h>
#include <EASTL/array.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Scene/Scene.hh>

#include <Assets/Model.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::renderer {

    inline constexpr i32 kInvalidTextureID{ -1 };
    inline constexpr u32 kMaxActiveLights{ 10000 };
    inline constexpr u32 kMaxSkinnedMeshes{ 1000 };
    inline constexpr u32 kMaxRenderableEntities{ 500'000 };

    // Material information
    struct MeshMaterialInfo {
        float4 mBaseColorFactor{};
        float4 mEmissiveFactor{};
        float4 mDiffuseFactor{};
        float4 mSpecularFactor{};

        f32 mMetallicFactor{};
        f32 mRoughnessFactor{};
        f32 mAlphaMask{};
        f32 mAlphaMaskCutoff{};
        f32 mEmissiveStrength{};

        i32 mBaseColorTextureSet{};
        i32 mMetallicRoughnessTextureSet{};
        i32 mSpecularGlossinessSet{};
        i32 mNormalTextureSet{};
        i32 mOcclusionTextureSet{};
        i32 mEmissiveTextureSet{};

        // Texture indices
        i32 mAlbedoIndex{ kInvalidTextureID };
        i32 mDiffuseIndex{ kInvalidTextureID };
        i32 mNormalIndex{ kInvalidTextureID };
        i32 mMetallicIndex{ kInvalidTextureID };
        i32 mRoughnessIndex{ kInvalidTextureID };
        i32 mAoIndex{ kInvalidTextureID };
        i32 mEmissiveIndex{ kInvalidTextureID };
        i32 mMetallicRoughnessIndex{ kInvalidTextureID };
        i32 mSpecularGlossinessIndex{ kInvalidTextureID };

        i32 mWorkflow{};

        i32 mIsBloomy{ MKT_SHADER_FALSE };
    };

    // Geometry information
    struct MeshGeometryInfo {
        float4x4 mTransform{};
        float4x4 mInverseModelView{}; // inverse(view * model) computed in CPU for performance

        // For vertex pulling, this tells the
        // offset into the array of vertices
        u32 mIndexOffset{};
        u32 mVertexOffset{};

        // Index into the buffer
        // of list of skinning matrices
        i32 mSkinningMatricesID{ -1 };
        i32 mHasArmature{ MKT_SHADER_FALSE };

        // TODO: add a list of shadow casters. This will be an index into the array of shadow casters
        // buffer to know from which lights this entity needs shadows casted from, ofc there will be a limit you cannot
        // just slap a unlimited amount of shadow casters
    };

    // Info that I pass per mesh
    // that needs to be animated
    struct MeshSkinningInfo {
        eastl::array<float4x4, asset::kMaxSkinnedMeshes> BoneTransforms{};
    };

    struct GeometryAllocation {
        u32 mVertexOffset{};
        u32 mVertexSize{};

        u32 mIndexOffset{};
        u32 mIndexSize{};

        u32 mOffsetID{};
    };

    class GeometryBufferAllocator {
    public:
        explicit GeometryBufferAllocator( u64 vertexBufferSize, u64 indexBufferSize );

        auto Free( const GeometryAllocation &alloc ) -> void;
        auto Allocate( u64 vertexBytes, u64 indexBytes ) -> eastl::optional<GeometryAllocation>;

    private:
        struct FreeRange {
            u64 mOffset{};
            u64 mSize{};
        };
        auto AllocateFrom( eastl::vector<FreeRange> &freeList, u64 size ) -> eastl::optional<u64>;

    private:
        eastl::vector<FreeRange> mVertexFreeList{};
        eastl::vector<FreeRange> mIndexFreeList{};
    };

    class GeometryAllocator {
    public:
        auto GetOrAllocate(const asset::MeshNode* mesh) -> GeometryAllocation;
        auto GetOrAllocate(const asset::MeshNode* mesh, GeometryAllocation& outAlloc) -> bool;

    private:
        GeometryBufferAllocator mAllocator{ MKT_MEGABYTES( 200 ), MKT_MEGABYTES( 200 ) };
        ankerl::unordered_dense::map<const asset::MeshNode*, GeometryAllocation> mAllocations{};
    };

    struct GeometryManagementModuleInfo {
        FGBufferHandle mVerticesBuffer{};
        FGBufferHandle mIndicesBuffer{};

        FGBufferHandle mMaterialsBuffer{};
        FGBufferHandle mGeometryBuffer{};
        FGBufferHandle mSkinningBuffer{};

        FGBufferHandle mIndirectBuffer{};

    };

    struct MeshNodeInstancesInfo {
        u32 mAllocationIndex{};
        u32 mInstanceCount{};

        GeometryAllocation mGeometryInfo{};

        eastl::vector<MeshGeometryInfo> mGeometryList{};
        eastl::vector<MeshMaterialInfo> mMaterialsList{};
    };

    struct GeometryBatch {

        auto Get( const asset::MeshNode* node, CommandContext& ctx, Blackboard& b  ) -> MeshNodeInstancesInfo&;

        GeometryAllocator mGeometryAllocator{};

        u32 mNodeAllocationIndex{};
        eastl::vector<u32> mMeshFreeNodeAllocationIndices{};
        ankerl::unordered_dense::map<const asset::MeshNode*, MeshNodeInstancesInfo> mInstanceCounts{};
    };

    class GeometryCullModule final {
    public:
        auto SetScene( const scene::Scene* scene) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto DrawInstancesIndirect( CommandContext& context ) -> void;

    private:
        auto InitGeometryData( CommandContext& ctx, Blackboard& b  ) -> void;

        auto RegisterMeshCullingPass(FrameGraph &graph) -> void;
        auto RegisterGeometryFilterPass(FrameGraph &graph) -> void;

        auto PrepareSkinning( CommandContext& context, Blackboard& b ) -> void;
        auto PrepareIndirectDraw( CommandContext& context, Blackboard& b  ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryBatch mBatch{};

        // Indirect draw
        static constexpr u32 kMaxIndirectCommands{ 1000000 };

        u32 mDrawCount{};
        FGBufferHandle mIndirectBuffer{};
        eastl::vector<DrawIndirectCommand> mIndirectCmds{};

        // Animation
        u32 mActiveAnimationCount{};
        eastl::vector<MeshSkinningInfo> mSkinningInfo{};
        ankerl::unordered_dense::set<u32> mActiveFinalMatsIndices{};
    };

}

#endif // MIKOTO_MESH_CULLING_HH
