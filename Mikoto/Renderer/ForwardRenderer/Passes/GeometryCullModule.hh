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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_GEOMETRY_CULL_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_GEOMETRY_CULL_MODULE_HH

#include <EASTL/span.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Assets/AssetModel.hh>

#include <Material/SurfaceMaterial.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/MeshRenderData.hh>
#include <Renderer/Core/SparseBufferUpload.hh>

#include <Scene/Scene.hh>

namespace mikoto::renderer {

    inline constexpr core::i32 kInvalidTextureID{ -1 };
    inline constexpr core::u32 kMaxActiveLights{ 16'384 };
    inline constexpr core::u32 kMaxUniqueModels{ 100'000 };
    inline constexpr core::u32 kMaxRenderableEntities{ 700'000 };

    inline constexpr core::u32 kVertexBufferSizeMB{ 1024 };
    inline constexpr core::u32 kIndexBufferSizeMB{ 1024 };

    enum class GeometryDrawGroup {
        eAll,
        eOpaqueAndMasked,
        eTransparent
    };

    // Material information
    struct alignas( 16 ) MeshMaterialInfo {
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

        core::i32 mPadding[2]{};
        // xyz: screen-space scatter scale, weight, shading model; w: cell bands.
        core::float4 mSubsurface{};
        core::float4 mSurfaceShading{};
        core::float4 mSubsurfaceColorOpacity{ 1.0f, 1.0f, 1.0f, 0.0f };
        core::i32 mClearCoatNormalIndex{ kInvalidTextureID };
        core::i32 mClearCoatNormalTextureSet{};
        core::f32 mClearCoatNormalScale{ 1.0f };
        core::f32 mNormalScale{ 1.0f };
        core::float4 mSubsurfaceTransmission{}; // Precomputed profile attenuation RGB.
        core::float4 mUvTransform{ 1, 1, 0, 0 };
        core::float4 mHeightMapping{};
        core::i32 mHeightIndex{-1};
        core::u32 mHeightUVSet{};
        core::u32 mHeightPadding[2]{};
        core::float4 mCheckerboardColorA{}; // RGB tint, enabled in W.
        core::float4 mCheckerboardColorB{}; // RGB tint, world-space cell size in W.
        core::float4 mCheckerboardOffset{};
    };

    // Geometry information
    struct alignas( 16 ) MeshGeometryInfo {
        core::float4x4 mTransform{};

        // Index into the buffer
        // of list of skinning matrices
        core::i32 mSkinningMatricesID{ -1 };

        // A mesh could not be animated but still have an armature that deforms it
        // the final pose is passed as a list of matrices we can fetch using mSkinningMatricesID
        core::i32 mHasArmature{ MKT_SHADER_FALSE };

        // Index into buffer of geometry info
        core::u32 mGeometryIndex{};

        // For entity selection
        core::u32 mObjectID{};

        core::u32 mIsHighlighted{};

        // Reuses GPU padding so shadow shaders can skip material/UV fetches for opaque casters.
        core::u32 mAlphaMode{};
        
        // Bit 0: double-sided materia.
        // Bit 1: mirrored instance transform.
        core::u32 mFaceFlags{};
        core::u32 mMaterialIndex{};

        // Mesh-local deformation shared by every instance using the same pose.
        // Zero selects the vertex-skinning fallback.
        rhi::DeviceAddress mSkinnedPositions{};
        rhi::DeviceAddress mSkinnedNormals{};
    };

    struct GeometryAllocation {
        // Each pointer addresses the first element of one mesh-local stream.
        // Non-indexed draws fetch their mesh-local index from mIndexBuffer,
        // then address only the attribute streams consumed by that pass.
        eastl::array<rhi::DeviceAddress, kMeshStreamCount> mStreams{};
        rhi::DeviceAddress mIndexBuffer{};
        core::u32 mAllocationIndex{};
        core::u32 mFirstIndex{};
        core::u32 mIndexCount{};
        core::u32 mPadding{};
    };

    class GeometryAllocator {
    public:
        auto Get( core::u32 index ) const -> const GeometryAllocation&;
        auto GetOrAllocate( const MeshRenderDataHandle& mesh ) -> const GeometryAllocation&;

        // Allocate storage in the preparation pass; record uploads in the next pass.
        auto Prepare( CommandContext& ctx, core::Blackboard& blackboard ) -> void;
        auto Flush( CommandContext& ctx, core::Blackboard& blackboard ) -> void;
        auto Clear() -> void;

        /** Mark allocations used by this view; inactive ranges are compacted after a grace period. */
        auto Touch( core::u32 index, core::u64 frame ) -> void;
        auto CollectUnused( core::u64 frame ) -> void;

    private:
        bool mRepack{};

        eastl::vector<core::u64> mLastSeen{};
        eastl::vector<core::u32> mFreeSlots{};

        // Immutable sources pin cache identities, preventing stale allocations
        // if a deleted MeshNode's address is reused by a later model import.
        eastl::vector<GeometryAllocation> mAllocations{};
        eastl::vector<MeshRenderDataHandle> mSources{};
        eastl::vector<core::u64> mUploadedRevisions{};
        ankerl::unordered_dense::map<const MeshRenderData*, core::u32> mAllocationIndices{};

        core::usize mIndexCount{};
        core::usize mUploadedCount{};
        core::usize mAttributeBytes{};
        core::usize mUploadedAttributeBytes{};
    };

    // CPU world-space results shared by every view of one scene. No component
    // pointers are retained, so EnTT storage moves cannot invalidate this cache.
    struct SceneGeometryCache {
        struct Entry {
            entt::entity mEntity{ entt::null };
            core::u64 mRevision{};
            core::u64 mWorldRevision{};
            core::u64 mMeshRevision{};
            MeshRenderDataHandle mMesh{};
            core::RefConst<asset::Model> mModel{}; // Pins the immutable mesh node and its skin binding.
            const asset::MeshNode* mNode{};
            bool mPosed{};
            core::float4x4 mTransform{ 1.0f };
            core::float3 mCenter{};
            core::float3 mExtent{};
            core::u32 mMirrored{};
        };

        // Entity index addresses the entry; the full handle rejects recycled generations.
        eastl::vector<Entry> mEntries{};

        MKT_NODISCARD auto Resolve( entt::entity entity ) -> Entry&;
    };

    struct GeometryCullModuleInfo {
        FGBufferHandle mGeometryAllocBuffer{};
        FGBufferHandle mMaterialsBuffer{};
        FGBufferHandle mSubsurfaceKernels{};
        FGBufferHandle mGeometryBuffer{}; // Per-view header, not an instance array.
        FGBufferHandle mInstanceBuffer{};
        FGBufferHandle mDrawInstanceBuffer{};
        FGBufferHandle mSkinningBuffer{};
        FGBufferHandle mSkinningCacheBuffer{};
        FGBufferHandle mAttributeBuffer{};
        FGBufferHandle mIndexBuffer{};

        FGSamplerHandle mBasicSampler{};
        FGSamplerHandle mMaterialSampler{};

        FGBufferHandle mIndirectBuffer{};
        FGBufferHandle mUnculledIndirectBuffer{};
        FGBufferHandle mDrawBoundsBuffer{};
    };

    class GeometryCullModule final {
    public:
        auto RegisterGeometryFilterPass( FrameGraph& graph ) -> void;
        auto RegisterSceneUploadPass( FrameGraph& graph ) -> void;
        auto RegisterSkinningPass( FrameGraph& graph ) -> void;
        auto RegisterFrustumCullPass( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;

        auto HasStylizedContours() const -> bool;

        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetFrustumCullingEnabled( bool enabled ) -> void;
        auto SetMeshesVisible( bool visible ) -> void;

        auto SetCullingDebugCamera( const scene::Camera* camera ) -> void;

        auto DrawInstancesIndirect(
            CommandContext& context,
            GeometryDrawGroup group = GeometryDrawGroup::eAll,
            bool cameraCulled = true ) -> void;

        // Appends visible instance runs while preserving the original packed instance IDs.
        // On capacity exhaustion, discards this view's suffix and returns false for an unculled fallback.
        auto AppendShadowDraws(
            const core::float4x4& viewProjection,
            eastl::vector<DrawIndirectCommand>& commands, core::usize capacity,
            core::u32& testedInstances, core::u32& culledInstances ) const -> bool;

        auto AppendHighlightedDraws( eastl::vector<DrawIndirectCommand>& commands ) const -> void;

        MKT_NODISCARD auto GetSharedMaterialCount() const -> core::usize;
        MKT_NODISCARD auto GetShadowCasterBatchCount() const -> core::u32;
        MKT_NODISCARD auto GetInstanceUploadStatistics() const -> const SceneUploadStatistics&;
        MKT_NODISCARD auto GetMaterialUploadStatistics() const -> const SceneUploadStatistics&;

    private:
        auto BuildDrawCommands() -> void;
        auto SortTransparentDraws( bool force ) -> core::u32;
        auto UpdateDrawBounds( CommandContext& context, FGBufferHandle buffer, core::u32 firstReordered ) -> void;
        auto UpdateDrawDebugFlags() -> void;

        auto ResetGeometryData() -> void;
        auto InitGeometryData( CommandContext& ctx, core::Blackboard& b ) -> void;

        auto UpdateMaterialClassification( core::u32 slot ) -> void;
        auto UpdateMaterials( CommandContext& ctx ) -> void;
        auto UpdateInstance( core::u32 slot, const SceneGeometryCache::Entry& derived,
                             core::u32 objectID, bool highlighted, core::i32 skinningPalette ) -> void;

        auto RetireUnusedSlots() -> void;
        auto ResolveMesh( core::u32 instanceSlot, const MeshRenderDataHandle& mesh ) -> void;

        auto CacheSkinning( MeshGeometryInfo& geometry, const MeshRenderData& mesh ) -> void;
        auto PrepareSkinningData( CommandContext& context, core::Blackboard& b ) -> void;
        auto UploadSkinningData( CommandContext& context, core::Blackboard& b ) -> void;
        auto RetireSkinningSlots() -> void;

        auto PrepareIndirectDraw( CommandContext& context, core::Blackboard& b ) -> void;
        auto UploadIndirectDraw( CommandContext& context, core::Blackboard& b ) -> void;

        MKT_NODISCARD auto ResolveInstance( entt::entity entity ) -> core::u32;
        MKT_NODISCARD auto FindInstance( entt::entity entity ) const -> core::u32;
        MKT_NODISCARD auto ResolveMaterial( const material::MaterialHandle &material, core::u32 cachedSlot ) -> core::u32;
        MKT_NODISCARD auto PushTextureID( CommandContext& ctx, renderer::rhi::TextureHandle handle ) -> core::i32;
        MKT_NODISCARD auto ResolveSkinningPalette( core::u64 animatorID, const asset::MeshNode &mesh, core::RefConst<asset::Model> model ) -> core::i32;

    private:
        struct SkinningPalette {
            eastl::array<core::float4x4, asset::kMaxBonesPerMesh> mBoneTransforms{};
        };
        struct SkinningSlot {
            core::RefConst<asset::Model> mModel{};
            core::u64 mRevision{};
            core::u64 mLastSeen{};
            bool mValid{};
        };
        struct SkinningJob {
            core::u32 mGeometryIndex{};
            core::u32 mPaletteIndex{};
            core::u32 mMaterialIndex{};
            core::u32 mVertexCount{};
            rhi::DeviceAddress mPositions{};
            rhi::DeviceAddress mNormals{};
        };

        struct TransparentDraw {
            core::u64 mKey{};
            DrawIndirectCommand mCommand{};
        };

        struct InstanceBounds {
            core::float3 mCenter{};
            core::float3 mExtent{};
            bool mCullable{};
        };

        struct MeshBatchInfo {
            core::usize mInstanceCount{};
            bool mBoundsDirty{};
            core::u32 mGeometryIndex{};
            eastl::vector<core::u32> mSlots{};
        };

        struct MaterialSlot {
            material::MaterialHandle mSource{};
            core::u64 mRevision{};
            core::u64 mLastSeen{};
            bool mUploaded{};
            core::u64 mClassificationRevision{};
            core::u32 mAlphaMode{};
            core::u32 mFaceFlags{};
            bool mClassificationValid{};
        };

        struct InstanceSlot {
            entt::entity mEntity{ entt::null };
            core::u64 mLastSeen{};
            InstanceBounds mBounds{};
            core::u32 mMaterialSlot{ kInvalidSlot };
            core::u32 mGeometryIndex{ kInvalidSlot };
            const MeshRenderData* mMesh{}; // Pinned by GeometryAllocator.
            core::u64 mDerivedRevision{};
            bool mUploaded{};
        };

        struct DrawInstance {
            core::u32 mSlot{};
            core::u32 mDebugFlags{};
        };

        struct alignas( 16 ) DrawBounds {
            core::float4 mCenter{};
            core::float4 mExtent{}; // w == 1 only if every instance is safely bounded.
        };

        // Draw commands index the packed draw-instance list, not persistent slots.
        MKT_NODISCARD auto GetInstanceBounds( core::u32 drawInstance ) const -> const InstanceBounds&;

    private:
        static constexpr core::u32 kInvalidSlot{ ~core::u32{} };
        static constexpr core::usize kSkinningCacheSizeMB{ 128 };

        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryAllocator mGeometryAllocator{};

        core::u64 mFrame{};
        eastl::shared_ptr<SceneGeometryCache> mSharedCache{};

        // Sparse entity-index -> GPU slot lookup. FindInstance also checks the
        // full entity generation in mInstanceStates before accepting a hit.
        // These slots belong to this view, never to a shared scene component.
        eastl::vector<core::u32> mInstanceSlots{};
        eastl::vector<InstanceSlot> mInstanceStates{};
        eastl::vector<MaterialSlot> mMaterialStates{};
        ankerl::unordered_dense::map<const material::IMaterial*, core::u32> mMaterialSlots{};

        eastl::vector<core::u32> mFreeInstances{};
        eastl::vector<core::u32> mFreeMaterials{};
        eastl::vector<DrawInstance> mDrawInstances{};

        SparseBufferUpload<MeshGeometryInfo> mInstanceUpload{};
        SparseBufferUpload<MeshMaterialInfo> mMaterialUpload{};
        SparseBufferUpload<material::SubsurfaceKernel> mSubsurfaceUpload{};

        FGBufferHandle mSubsurfaceScratch{};
        eastl::vector<material::SubsurfaceKernel> mSubsurfaceKernels{};
        SparseBufferUpload<DrawInstance> mDrawInstanceUpload{};

        FGBufferHandle mInstanceScratch{};
        FGBufferHandle mMaterialScratch{};
        FGBufferHandle mDrawInstanceScratch{};

        eastl::vector<MeshGeometryInfo> mGeometryList{};
        bool mHasStylizedContours{};
        eastl::vector<MeshMaterialInfo> mMaterialsList{};

        // Indexed by the allocator's persistent geometry index.
        eastl::vector<MeshBatchInfo> mMeshBatchInfos{};

        // Indirect draw
        core::u32 mOpaqueDrawCount{};
        FGBufferHandle mIndirectBuffer{};
        FGBufferHandle mUnculledIndirectBuffer{};
        eastl::vector<TransparentDraw> mTransparentDraws{};

        bool mMeshesVisible{ true };
        bool mFrustumCullingEnabled{ true };
        const scene::Camera* mCullingDebugCamera{};
        
        eastl::vector<DrawBounds> mDrawBounds{};
        eastl::vector<core::u32> mDirtyDrawBounds{};
        eastl::vector<DrawIndirectCommand> mIndirectCmds{};
        
        core::u32 mFirstChangedDraw{};

        // Membership/alpha edits rebuild commands; movement dirties only its mesh batch.
        bool mCommandsDirty{ true };
        bool mHadCullingDebugCamera{};

        rhi::DeviceAddress mSkinningCacheAddress{};
        eastl::vector<SkinningPalette> mSkinningInfo{};
        ankerl::unordered_dense::map<std::pair<core::u64, const asset::MeshNode*>, core::u32> mSkinningSlots{};
        eastl::vector<SkinningSlot> mSkinningStates{};
        eastl::vector<core::u32> mFreeSkinningSlots{};
        eastl::vector<core::u32> mDirtySkinningSlots{};
        eastl::vector<SkinningJob> mSkinningJobs{};
        ankerl::unordered_dense::map<std::pair<core::u32, core::u64>, core::u32> mSkinningJobIndices{};
        core::usize mSkinningCacheBytes{};
        bool mWarnedSkinningCacheCapacity{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_GEOMETRY_CULL_MODULE_HH
