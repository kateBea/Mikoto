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

#ifndef MIKOTO_ASSETS_ASSET_MODEL_HH
#define MIKOTO_ASSETS_ASSET_MODEL_HH

#include <vector>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Math/PointBoundsTree.hh>

#include <Filesystem/File.hh>

#include <Assets/Asset.hh>
#include <Assets/Utility.hh>
#include <Assets/Importer.hh>

#include <Material/SurfaceMaterial.hh>

#include <Animation/SkinningBounds.hh>
#include <Animation/SkinningPalette.hh>
#include <Animation/SkinnedAnimation.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/MeshRenderData.hh>

namespace mikoto::asset {

    using AnimationList = ankerl::unordered_dense::map<eastl::string, eastl::unique_ptr<animation::SkinnedAnimation>>;

    /** Immutable imported rig data. Instances keep their poses in Animator. */
    struct ModelRig {
        eastl::string mName{};
        eastl::unique_ptr<animation::Skeleton> mSkeleton{};
        AnimationList mAnimations{};
    };

    struct ModelDataDescription {
        eastl::string mName{};

        eastl::vector<MeshNodeDescription> mMeshNodes{};
        eastl::vector<material::SurfaceMaterialDescription> mMaterials{};

        eastl::vector<ModelRig> mRigs{};
        AnimationList mAnimations{};
        eastl::unique_ptr<animation::Skeleton> mSkeleton{};

        // Texture URI the same way is stored in the materials
        ankerl::unordered_dense::map<filesystem::Path, renderer::rhi::TextureHandle> mTextures{};
    };

    // These 2 are vec4 because they need to match the bone influence
    // which is maximum bones influence a vertex ( from what we support now )
    inline constexpr core::u32 kMaxBoneInfluence{ 4 };
    inline constexpr core::u32 kMaxBonesPerMesh{ 512 }; // Needs to match shader's

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

    struct ImportConfiguration {
        // Mesh options
        bool mImportMesh{ true };

        // Animations options
        bool mImportSkeletons{ true };
        bool mImportAnimations{ true };

        // Transform options
        bool mImportTranslation{ true };
        bool mImportRotation{ true };
        bool mImportScale{ true };

        // Materials options
        bool mImportTextures{ true };
        bool mImportMaterials{ true };

        /** @brief Persists settings for future streaming and reloads.
         * @param model Source model path whose adjacent .mktimport file is written.
         */
        auto Save( const filesystem::Path& model ) const -> void;

        /** @brief Reads optional import settings beside the model; missing files use defaults.
         * @param model Source model path. Settings use the adjacent .mktimport file.
         * @return Saved settings, or the default import configuration.
         */
        MKT_NODISCARD static auto Load( const filesystem::Path& model ) -> ImportConfiguration;
        MKT_NODISCARD auto operator==( const ImportConfiguration& ) const -> bool = default;

        MKT_NODISCARD constexpr auto SetImportMesh( bool enable ) noexcept -> ImportConfiguration& { mImportMesh = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportSkeletons( bool enable ) noexcept -> ImportConfiguration& { mImportSkeletons = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportAnimations( bool enable ) noexcept -> ImportConfiguration& { mImportAnimations = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportTranslation( bool enable ) noexcept -> ImportConfiguration& { mImportTranslation = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportRotation( bool enable ) noexcept -> ImportConfiguration& { mImportRotation = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportScale( bool enable ) noexcept -> ImportConfiguration& { mImportScale = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportTextures( bool enable ) noexcept -> ImportConfiguration& { mImportTextures = enable; return *this; }
        MKT_NODISCARD constexpr auto SetImportMaterials( bool enable ) noexcept -> ImportConfiguration& { mImportMaterials = enable; return *this; }
    };

    struct ModelLoadDescription {
        filesystem::FileHandle mFile{};
        renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eInvalid };
        bool mExtractTextures{ true };

        // Keep imported texture references but leave their residency to materials.
        bool mDeferTextures{ true };

        ImportConfiguration mImportConfig{};
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };

        auto LoadTextures( bool value ) -> ModelLoadDescription&;
        auto WithFilePath( filesystem::FileHandle file ) -> ModelLoadDescription&;
    };

    struct MeshCreateDescription {
        renderer::MeshRenderDataHandle mRenderData{};
        renderer::rhi::BufferHandle mVertices{};
        renderer::rhi::BufferHandle mIndices{};

        core::float4x4 mTransform{};
        eastl::string_view mName{};
        material::SurfaceMaterialDescription mProperties{};
        core::u32 mMaterialSourceIndex{ ~core::u32{} };
        // CPU geometry is retained once per mesh for collision cooking.
        std::vector<core::float3> mCollisionVertices{};
        std::vector<core::u32> mCollisionIndices{};
        std::vector<animation::SkinningVertex> mSkinningVertices{};
        animation::MeshSkinBinding mSkinBinding{};

        auto SetName( eastl::string_view name ) -> MeshCreateDescription&;
        auto SetTransform( const core::float4x4& t ) -> MeshCreateDescription&;
        auto SetMaterial( const material::SurfaceMaterialDescription& mat ) -> MeshCreateDescription&;
        auto SetVertices( renderer::rhi::BufferHandle vertices ) -> MeshCreateDescription&;
        auto SetIndices( renderer::rhi::BufferHandle indices ) -> MeshCreateDescription&;
    };

    class MeshNode final : public core::INonCopyable {
    public:
        MeshNode(core::u32 index, const MeshCreateDescription& desc);
        MeshNode(asset::MeshNode&& other) noexcept = default;

        MKT_NODISCARD auto GetRenderData() const -> const renderer::MeshRenderDataHandle&;
        MKT_NODISCARD auto GetSkinnedBounds( eastl::span<const core::float4x4> palette,
                                             const core::float4x4& transform ) const -> math::Bounds;

        MKT_NODISCARD auto GetName() -> const eastl::string&;

        MKT_NODISCARD auto GetTransform() const -> const core::float4x4&;
        MKT_NODISCARD auto GetMeshIndex() const -> core::usize;
        MKT_NODISCARD auto GetVertexBuffer() -> renderer::rhi::BufferHandle;
        MKT_NODISCARD auto GetIndexBuffer() -> renderer::rhi::BufferHandle;

        MKT_NODISCARD auto GetVertexBuffer() const -> renderer::rhi::BufferHandle;
        MKT_NODISCARD auto GetIndexBuffer() const -> renderer::rhi::BufferHandle;

        MKT_NODISCARD auto GetProperties() const -> const material::SurfaceMaterialDescription&;
        MKT_NODISCARD auto GetMaterialSourceIndex() const -> core::u32;
        MKT_NODISCARD auto GetCollisionVertices() const -> const std::vector<core::float3>&;
        MKT_NODISCARD auto GetTransformedBounds( const core::float4x4& transform ) const -> math::Bounds;
        MKT_NODISCARD auto GetCollisionIndices() const -> const std::vector<core::u32>&;
        MKT_NODISCARD auto GetSkinningVertices() const -> const std::vector<animation::SkinningVertex>&;
        MKT_NODISCARD auto GetSkinBinding() const -> const animation::MeshSkinBinding&;

    private:
        eastl::string mName{};
        core::usize mMeshIndex{};

        renderer::rhi::BufferHandle mIndices{};
        renderer::rhi::BufferHandle mVertices{};
        renderer::MeshRenderDataHandle mRenderData{};

        material::SurfaceMaterialDescription mProperties{};
        core::u32 mMaterialSourceIndex{ ~core::u32{} };

        core::float4x4 mTransform{ 1.0f }; // Identity by default
        std::vector<core::float3> mCollisionVertices{};
        math::PointBoundsTree mBoundsTree;
        std::vector<core::u32> mCollisionIndices{};
        std::vector<animation::SkinningVertex> mSkinningVertices{};
        animation::MeshSkinBinding mSkinBinding{};
        animation::SkinningBounds mSkinningBounds{};
    };

    struct ModelCreateDescription {
        filesystem::Path mPath{};
        eastl::string mName{};

        AnimationList mAnimations{};
        eastl::unique_ptr<animation::Skeleton> mSkeleton{};
        eastl::vector<ModelRig> mRigs{};
        ankerl::unordered_dense::map<core::u32, MeshNode> mMeshes{};

        auto SetPath(const filesystem::Path& path) -> ModelCreateDescription&;
        auto AddMesh(core::u32 index, const MeshCreateDescription& desc) -> ModelCreateDescription&;
        auto SetName(eastl::string_view name) -> ModelCreateDescription&;
        auto SetSkeleton(eastl::unique_ptr<animation::Skeleton>&& skeleton) -> ModelCreateDescription&;
        auto SetAnimations(AnimationList&& animations) -> ModelCreateDescription&;
    };

    class Model final : public core::INonCopyable, public core::INonMoveable, public core::ReferenceCounted<Model> {
    public:
        explicit Model( ModelCreateDescription&& desc );

        /** Cached payload estimate for streaming budgets; does not include scene arenas. */
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;
        MKT_NODISCARD auto GetMeshNodeCount() const -> core::usize;

        MKT_NODISCARD auto GetMeshNode( core::usize index) -> MeshNode&;
        MKT_NODISCARD auto GetMeshNode( core::usize index) const -> const MeshNode&;

        MKT_NODISCARD auto GetPath() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto IsSkinned() const -> bool;
        MKT_NODISCARD auto HasArmature() const -> bool;
        MKT_NODISCARD auto HasAnimations() const -> bool;

        /** @brief Gets a rig's immutable skeleton; invalid indices return nullptr. */
        MKT_NODISCARD auto GetSkeleton( core::u32 rig = 0 ) const -> const animation::Skeleton*;

        MKT_NODISCARD auto FindAnimation( eastl::string_view name, core::u32 rig = 0 ) -> animation::SkinnedAnimation*;
        MKT_NODISCARD auto FindAnimation( eastl::string_view name, core::u32 rig = 0 ) const -> const animation::SkinnedAnimation*;

        /** @brief Gets clips bound to one rig, or an empty list for an invalid index.
         * Imported glTF rigs expose the same clip names and source durations,
         * including rest tracks when a clip does not animate that rig.
         */
        MKT_NODISCARD auto GetAnimations( core::u32 rig = 0 ) const -> const AnimationList&;

        /** Stable model-local indices used by MeshSkinBinding::mRigIndex. */
        MKT_NODISCARD auto GetRigs() const -> const eastl::vector<ModelRig>&;

        ~Model() override = default;

    private:
        // Only the factory can construct models
        friend class MeshFactory;

    protected:
        AssetMemoryUsage mMemoryUsage{};
        filesystem::Path mPath{};
        eastl::string mName{};

        eastl::vector<ModelRig> mRigs{};

        // ( Mesh index, mesh node )
        ankerl::unordered_dense::map<core::u32, MeshNode> mMeshes{};
    };

    using ModelHandle = core::Ref<Model>;

    template<> struct AssetTraits<Model> {
        using Asset = Model;
        using Description = ModelLoadDescription;

        static constexpr AssetType kType{ AssetType::eModel };
    };
}

#endif// MIKOTO_ASSETS_ASSET_MODEL_HH
