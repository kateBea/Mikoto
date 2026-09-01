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

    inline constexpr core::i32 kInvalidTextureID{ -1 };
    inline constexpr core::u32 kMaxActiveLights{ 10000 };
    inline constexpr core::u32 kMaxSkinnedMeshes{ 1000 };
    inline constexpr core::u32 kMaxUniqueModels{ 100'000 };
    inline constexpr core::u32 kMaxRenderableEntities{ 500'000 };

    inline constexpr core::u32 kVertexBufferSizeMB{ 512 };
    inline constexpr core::u32 kIndexBufferSizeMB{ 512 };

    // Material information
    struct alignas(16) MeshMaterialInfo {
        core::float4 mBaseColorFactor{};
        core::float4 mEmissiveFactor{};
        core::float4 mDiffuseFactor{};
        core::float4 mSpecularFactor{};

        core::f32 mMetallicFactor{};
        core::f32 mRoughnessFactor{};
        core::f32 mAlphaMask{};
        core::f32 mAlphaMaskCutoff{};
        core::f32 mEmissiveStrength{};

        core::i32 mBaseColorTextureSet{};
        core::i32 mMetallicRoughnessTextureSet{};
        core::i32 mSpecularGlossinessSet{};
        core::i32 mNormalTextureSet{};
        core::i32 mOcclusionTextureSet{};
        core::i32 mEmissiveTextureSet{};

        // Texture indices
        core::i32 mAlbedoIndex{ kInvalidTextureID };
        core::i32 mDiffuseIndex{ kInvalidTextureID };
        core::i32 mNormalIndex{ kInvalidTextureID };
        core::i32 mMetallicIndex{ kInvalidTextureID };
        core::i32 mRoughnessIndex{ kInvalidTextureID };
        core::i32 mAoIndex{ kInvalidTextureID };
        core::i32 mEmissiveIndex{ kInvalidTextureID };
        core::i32 mMetallicRoughnessIndex{ kInvalidTextureID };
        core::i32 mSpecularGlossinessIndex{ kInvalidTextureID };

        core::i32 mIsBloomy{ MKT_SHADER_FALSE };

        core::i32 mWorkflow{};

        core::i32 mPadding[2];
    };

    // Geometry information
    struct alignas(16) MeshGeometryInfo {
        core::float4x4 mTransform{};
        core::float4x4 mInverseModelView{};// inverse(view * model) computed in CPU for performance

        // Index into the buffer
        // of list of skinning matrices
        core::i32 mSkinningMatricesID{ -1 };

        // A mesh could not be animated but still have an armature that deforms it
        // the final pose is passed as a list of matrices we can fetch using mSkinningMatricesID
        core::i32 mHasArmature{ MKT_SHADER_FALSE };

        // TODO: add a list of shadow casters. This will be an index into the array of shadow casters
        // buffer to know from which lights this entity needs shadows casted from, ofc there will be a limit you cannot
        // just slap a unlimited amount of shadow casters

        // Index into buffer of geometry info
        core::u32 mGeometryIndex{};

        // For entity selection
        core::u32 mObjectID{};

        core::u32 mPadding{};
    };

    // Info that I pass per mesh
    // that needs to be animated or deformed
    // with some skinning matrices
    struct MeshSkinningInfo {
        eastl::array<core::float4x4, asset::kMaxBonesPerMesh> mBoneTransforms{};
    };

    struct GeometryAllocation {
        core::u64 mAllocationIndex{};
        rhi::DeviceAddress mVertexBuffer{};
        rhi::DeviceAddress mIndexBuffer{};
    };

    class GeometryAllocator {
    public:
        explicit GeometryAllocator();

        auto GetOrAllocate( const asset::MeshNode* mesh ) -> GeometryAllocation;

        auto Flush( CommandContext& ctx, Blackboard& blackboard ) -> void;

    private:
        bool mFlushRequired{};
        core::u64 mCurrentAllocationIndex{};
        eastl::vector<GeometryAllocation> mAllocations{};
        ankerl::unordered_dense::map<const asset::MeshNode*, core::u64> mAllocationIndices{};
    };

    struct GeometryBatch {
        auto Get( const asset::MeshNode* node, CommandContext& ctx, Blackboard& b ) -> GeometryAllocation&;

        GeometryAllocator mGeometryAllocator{};
        ankerl::unordered_dense::map<const asset::MeshNode*, GeometryAllocation> mInstanceCounts{};
    };

    struct GeometryCullModuleInfo {
        FGBufferHandle mGeometryAllocBuffer{};
        FGBufferHandle mMaterialsBuffer{};
        FGBufferHandle mGeometryBuffer{};
        FGBufferHandle mSkinningBuffer{};

        FGSamplerHandle mBasicSampler{};

        FGBufferHandle mIndirectBuffer{};
    };

    class GeometryCullModule final {
    public:
        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto DrawInstancesIndirect( CommandContext& context ) -> void;

    private:
        auto InitGeometryData( CommandContext& ctx, Blackboard& b ) -> void;

        auto RegisterMeshCullingPass( FrameGraph& graph ) -> void;
        auto RegisterGeometryFilterPass( FrameGraph& graph ) -> void;

        auto PrepareSkinning( CommandContext& context, Blackboard& b ) -> void;
        auto PrepareIndirectDraw( CommandContext& context, Blackboard& b ) -> void;

        auto PushTextureID( CommandContext& ctx, TextureHandle handle ) -> core::i32;

    private:
        struct MeshBatchInfo {
            core::usize mInstanceCount{};
            eastl::vector<MeshGeometryInfo> mGeometryList{};
            eastl::vector<MeshMaterialInfo> mMaterialsList{};
        };

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryBatch mBatch{};
        GeometryAllocator mGeometryAllocator{};

        eastl::vector<MeshGeometryInfo> mGeometryList{};
        eastl::vector<MeshMaterialInfo> mMaterialsList{};

        ankerl::unordered_dense::map<const asset::MeshNode*, MeshBatchInfo> mMeshBatchInfos{};

        // Indirect draw
        static constexpr core::u32 kMaxIndirectCommands{ 1000000 };

        core::u32 mDrawCount{};
        FGBufferHandle mIndirectBuffer{};
        eastl::vector<DrawIndirectCommand> mIndirectCmds{};

        // Animation
        core::u32 mActiveAnimationCount{};
        eastl::vector<MeshSkinningInfo> mSkinningInfo{};
        ankerl::unordered_dense::set<core::u32> mActiveFinalMatsIndices{};
    };
}// namespace mikoto::renderer

#endif // MIKOTO_MESH_CULLING_HH
