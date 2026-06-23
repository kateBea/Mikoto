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

#include <EASTL/algorithm.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Assets/Model.hh>

#include <Memory/Allocator.hh>

namespace mikoto::asset {

    auto ModelLoadDescription::WithFilePath( FileHandle file ) -> ModelLoadDescription & {
        this->mFile = file;
        return *this;
    }

    auto ModelLoadDescription::LoadTextures( bool value ) -> ModelLoadDescription & {
        this->mExtractTextures = value;
        return *this;
    }

    auto MeshCreateDescription::SetName( eastl::string_view name ) -> MeshCreateDescription & {
        mName = name;
        return *this;
    }

    auto MeshCreateDescription::SetTransform( const float4x4 &t ) -> MeshCreateDescription & {
        mTransform = t;
        return *this;
    }

    auto MeshCreateDescription::SetMaterial( const PhysicMaterialDescription &mat ) -> MeshCreateDescription & {
        mProperties = mat;
        return *this;
    }

    auto MeshCreateDescription::SetVertices( BufferHandle vertices ) -> MeshCreateDescription & {
        mVertices = vertices;
        return *this;
    }

    auto MeshCreateDescription::SetIndices( BufferHandle indices ) -> MeshCreateDescription & {
        mIndices = indices;
        return *this;
    }

    MeshNode::MeshNode( u32 index, const MeshCreateDescription& desc)
        : mName{ desc.mName },
          mMeshIndex{ index },
          mIndices{ desc.mIndices },
          mVertices{ desc.mVertices },
          mProperties{ desc.mProperties },
          mTransform{ desc.mTransform } {}

    auto ModelCreateDescription::SetPath( const Path &path ) -> ModelCreateDescription & {
        mPath = path;
        return *this;
    }

    auto ModelCreateDescription::AddMesh( u32 index, const MeshCreateDescription& desc ) -> ModelCreateDescription & {
        mMeshes.try_emplace( index, index, desc );
        return *this;
    }

    auto ModelCreateDescription::SetName( eastl::string_view name ) -> ModelCreateDescription & {
        mName = name;
        return *this;
    }

    auto ModelCreateDescription::SetSkeleton( eastl::unique_ptr<Skeleton>&& skeleton ) -> ModelCreateDescription & {
        mSkeleton = std::move( skeleton );
        return *this;
    }

    auto ModelCreateDescription::SetAnimations( AnimationList &&animations ) -> ModelCreateDescription & {
        mAnimations = std::move( animations );
        return *this;
    }

    auto Model::GetMeshNodeCount() const -> size_t {
        return mMeshes.size();
    }

    auto Model::GetMeshNode( const size_t index ) -> MeshNode& {
        return mMeshes.at(index);
    }

    auto Model::GetMeshNode( const size_t index ) const -> const MeshNode& {
        return mMeshes.at(index);
    }

    auto Model::GetPath() const -> const Path& {
        return mPath;
    }

    auto Model::GetName() const -> eastl::string_view {
        return mName;
    }

    auto Model::IsSkinned() const -> bool {
        return !mAnimations.empty(); // TODO, a model is skinned if it has joints, some models do not have joints at all
        // some others just have joints but then they are just stuck in a specific pose if there is no animation
    }

    auto Model::HasArmature() const -> bool {
        return mSkeleton->IsArmaturePresent();
    }

    auto Model::HasAnimations() const -> bool {
        return !mAnimations.empty();
    }

    auto Model::GetSkeleton() const -> const Skeleton* {
        return mSkeleton.get();
    }

    auto Model::GetAnimations() const -> const AnimationList& {
        return mAnimations;
    }

    auto Model::GetAnimations() -> AnimationList& {
        return mAnimations;
    }

    auto Model::FindAnimation( eastl::string_view name ) -> SkinnedAnimation * {
        return const_cast<SkinnedAnimation*>( std::as_const(*this).FindAnimation(name) );
    }

    auto Model::FindAnimation( eastl::string_view name ) const -> const SkinnedAnimation * {
        const auto it{ mAnimations.find( Path{ name } ) };
        if (it != mAnimations.end()) {
            return it->second.get();
        }

        return nullptr;
    }
}