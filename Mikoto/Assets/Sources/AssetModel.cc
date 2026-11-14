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

#include <ranges>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>
#include <EASTL/unordered_set.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>

#include <Assets/AssetModel.hh>

#include <Renderer/Rhi/Utility.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::material;
    using namespace mikoto::animation;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    auto ModelLoadDescription::WithFilePath( filesystem::FileHandle file ) -> ModelLoadDescription& {
        MKT_BEGIN_PROFILER();
        this->mFile = file;
        mPath = mFile ? mFile->GetPath() : filesystem::Path{};
        return *this;
    }

    auto ModelLoadDescription::LoadTextures( bool value ) -> ModelLoadDescription& {
        MKT_BEGIN_PROFILER();
        this->mExtractTextures = value;
        return *this;
    }

    auto MeshCreateDescription::SetName( eastl::string_view name ) -> MeshCreateDescription& {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto MeshCreateDescription::SetTransform( const core::float4x4& t ) -> MeshCreateDescription& {
        MKT_BEGIN_PROFILER();
        mTransform = t;
        return *this;
    }

    auto MeshCreateDescription::SetMaterial( const material::SurfaceMaterialDescription& mat ) -> MeshCreateDescription& {
        MKT_BEGIN_PROFILER();
        mProperties = mat;
        return *this;
    }

    auto MeshCreateDescription::SetVertices( renderer::rhi::BufferHandle vertices ) -> MeshCreateDescription& {
        MKT_BEGIN_PROFILER();
        mVertices = vertices;
        return *this;
    }

    auto MeshCreateDescription::SetIndices( renderer::rhi::BufferHandle indices ) -> MeshCreateDescription& {
        MKT_BEGIN_PROFILER();
        mIndices = indices;
        return *this;
    }

    MeshNode::MeshNode( core::u32 index, const MeshCreateDescription& desc )
        : mName{ desc.mName },
          mMeshIndex{ index },
          mIndices{ desc.mIndices },
          mVertices{ desc.mVertices },
          mRenderData{ desc.mRenderData },
          mProperties{ desc.mProperties },
          mMaterialSourceIndex{ desc.mMaterialSourceIndex },
          mTransform{ desc.mTransform },
          mCollisionVertices{ desc.mCollisionVertices },
          mCollisionIndices{ desc.mCollisionIndices },
          mSkinningVertices{ desc.mSkinningVertices },
          mSkinBinding{ desc.mSkinBinding } {
        MKT_BEGIN_PROFILER();
        mBoundsTree.Build( mCollisionVertices );

        if ( !mSkinningVertices.empty() ) {
            mSkinningBounds.Build( { mSkinningVertices.data(), mSkinningVertices.size() }, kMaxBonesPerMesh );
        }
    }

    auto MeshNode::GetSkinnedBounds( eastl::span<const core::float4x4> palette,
        const core::float4x4& transform ) const -> math::Bounds {
        return mSkinningBounds.Transform( palette, transform );
    }

    auto MeshNode::GetRenderData() const -> const renderer::MeshRenderDataHandle& {
        return mRenderData;
    }

    auto ModelCreateDescription::SetPath( const filesystem::Path& path ) -> ModelCreateDescription& {
        MKT_BEGIN_PROFILER();
        mPath = path;
        return *this;
    }

    auto ModelCreateDescription::AddMesh( core::u32 index, const MeshCreateDescription& desc ) -> ModelCreateDescription& {
        MKT_BEGIN_PROFILER();
        mMeshes.try_emplace( index, index, desc );
        return *this;
    }

    auto ModelCreateDescription::SetName( eastl::string_view name ) -> ModelCreateDescription& {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto ModelCreateDescription::SetSkeleton( eastl::unique_ptr<animation::Skeleton>&& skeleton ) -> ModelCreateDescription& {
        MKT_BEGIN_PROFILER();
        mSkeleton = eastl::move( skeleton );
        return *this;
    }

    auto ModelCreateDescription::SetAnimations( AnimationList&& animations ) -> ModelCreateDescription& {
        MKT_BEGIN_PROFILER();
        mAnimations = eastl::move( animations );
        return *this;
    }

    Model::Model( ModelCreateDescription&& desc )
        : mPath{ eastl::move( desc.mPath ) },
          mName{ eastl::move( desc.mName ) },
          mRigs{ eastl::move( desc.mRigs ) },
          mMeshes{ eastl::move( desc.mMeshes ) } {
        // Assimp and older callers still describe one rig. Normalize that path
        // here so all runtime consumers use the same model representation.
        if ( mRigs.empty() && desc.mSkeleton ) {
            mRigs.push_back( { mName, eastl::move( desc.mSkeleton ), eastl::move( desc.mAnimations ) } );
        }
        // Count shared primitive and texture payloads once per model. These are
        // logical byte estimates, not a replacement for OS process residency.
        eastl::unordered_set<const void*> counted;

        for ( const auto& [index, mesh]: mMeshes ) {
            for ( const auto& buffer: { mesh.GetVertexBuffer(), mesh.GetIndexBuffer() } ) {
                if ( buffer && counted.insert( buffer.GetPtr() ).second ) {
                    mMemoryUsage.mGpuBytes += buffer->GetSizeBytes();
                }
            }

            const auto& render{ mesh.GetRenderData() };

            if ( render && counted.insert( render.GetPtr() ).second ) {
                mMemoryUsage.mCpuBytes += render->GetAttributes().size() + render->GetIndices().size_bytes();
            }

            mMemoryUsage.mCpuBytes += mesh.GetCollisionVertices().capacity() * sizeof( core::float3 );
            mMemoryUsage.mCpuBytes += mesh.GetCollisionIndices().capacity() * sizeof( core::u32 );

            for ( const auto& [path, map]: mesh.GetProperties().mTextureBindings ) {
                if ( !map.mTexture || !counted.insert( map.mTexture.GetPtr() ).second ) {
                    continue;
                }

                const auto& format{ renderer::rhi::GetFormatInfo( map.mTexture->GetFormat() ) };
                const core::usize block{ eastl::max<core::usize>( 1, format.mBlockSize ) };
                core::usize width{ map.mTexture->GetWidth() };
                core::usize height{ map.mTexture->GetHeight() };

                for ( core::u32 mip{}; mip < map.mTexture->GetMipLevelCount(); ++mip ) {
                    mMemoryUsage.mGpuBytes += ( ( width + block - 1 ) / block ) * ( ( height + block - 1 ) / block ) * format.mBytesPerBlock;
                    width = eastl::max<core::usize>( 1, width / 2 );
                    height = eastl::max<core::usize>( 1, height / 2 );
                }
            }
        }
    }

    auto Model::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return mMemoryUsage;
    }

    auto Model::GetMeshNodeCount() const -> core::usize {
        return mMeshes.size();
    }

    auto Model::GetMeshNode( const core::usize index ) -> MeshNode& {
        return mMeshes.at( index );
    }

    auto Model::GetMeshNode( const core::usize index ) const -> const MeshNode& {
        return mMeshes.at( index );
    }

    auto Model::GetPath() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mPath;
    }

    auto Model::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto Model::IsSkinned() const -> bool {
        MKT_BEGIN_PROFILER();
        // Skinning needs a skeleton, not a clip. Skeleton-only models still
        // evaluate their rest pose and can be deformed by IK. Imported rigid
        // animation nodes also use this pose path to follow their ancestors.
        return HasArmature();
    }

    auto Model::HasArmature() const -> bool {
        return !mRigs.empty() && mRigs.front().mSkeleton && mRigs.front().mSkeleton->IsArmaturePresent();
    }

    auto Model::HasAnimations() const -> bool {
        return !GetAnimations().empty();
    }

    auto Model::GetRigs() const -> const eastl::vector<ModelRig>& {
        return mRigs;
    }

    auto Model::GetSkeleton( core::u32 rig ) const -> const animation::Skeleton* {
        return rig < mRigs.size() ? mRigs[rig].mSkeleton.get() : nullptr;
    }

    auto Model::GetAnimations( core::u32 rig ) const -> const AnimationList& {
        static const AnimationList empty{};
        return rig < mRigs.size() ? mRigs[rig].mAnimations : empty;
    }

    auto Model::FindAnimation( eastl::string_view name, core::u32 rig ) -> animation::SkinnedAnimation* {
        return const_cast<animation::SkinnedAnimation*>( std::as_const( *this ).FindAnimation( name, rig ) );
    }

    auto Model::FindAnimation( eastl::string_view name, core::u32 rig ) const -> const animation::SkinnedAnimation* {
        const auto& clips{ GetAnimations( rig ) };
        const auto it{ clips.find( eastl::string{ name } ) };
        return it != clips.end() ? it->second.get() : nullptr;
    }


    auto MeshNode::GetName() -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mName;
    }


    auto MeshNode::GetTransform() const -> const core::float4x4& {
        MKT_BEGIN_PROFILER();
        return mTransform;
    }


    auto MeshNode::GetMeshIndex() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mMeshIndex;
    }


    auto MeshNode::GetVertexBuffer() -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        return mVertices;
    }


    auto MeshNode::GetIndexBuffer() -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        return mIndices;
    }


    auto MeshNode::GetVertexBuffer() const -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        return mVertices;
    }


    auto MeshNode::GetIndexBuffer() const -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        return mIndices;
    }


    auto MeshNode::GetProperties() const -> const material::SurfaceMaterialDescription& {
        MKT_BEGIN_PROFILER();
        return mProperties;
    }


    auto MeshNode::GetMaterialSourceIndex() const -> core::u32 {
        return mMaterialSourceIndex;
    }


    auto MeshNode::GetCollisionVertices() const -> const std::vector<core::float3>& {
        MKT_BEGIN_PROFILER();
        return mCollisionVertices;
    }


    auto MeshNode::GetTransformedBounds( const core::float4x4& transform ) const -> math::Bounds {
        MKT_BEGIN_PROFILER();
        return mBoundsTree.Transform( mCollisionVertices, transform );
    }


    auto MeshNode::GetCollisionIndices() const -> const std::vector<core::u32>& {
        MKT_BEGIN_PROFILER();
        return mCollisionIndices;
    }


    auto MeshNode::GetSkinningVertices() const -> const std::vector<animation::SkinningVertex>& {
        MKT_BEGIN_PROFILER();
        return mSkinningVertices;
    }


    auto MeshNode::GetSkinBinding() const -> const animation::MeshSkinBinding& {
        return mSkinBinding;
    }
}
