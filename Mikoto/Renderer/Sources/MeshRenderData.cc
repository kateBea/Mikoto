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

#include <cstring>

#include <EASTL/numeric_limits.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Assets/Importer.hh>

#include <Renderer/Core/MeshRenderData.hh>

namespace mikoto::renderer {
    static constexpr core::u64 kMeshTopologyHashSeed{ 1469598103934665603ull };
    static constexpr core::u64 kMeshTopologyHashPrime{ 1099511628211ull };

    MeshRenderData::MeshRenderData( const MeshRenderData& source )
        : mIndices{ source.mIndices },
          mAttributes{ source.mAttributes },
          mOffsets{ source.mOffsets },
          mSortCenter{ source.mSortCenter },
          mBoundsExtent{ source.mBoundsExtent },
          mTopologyHash{ source.mTopologyHash },
          mVertexCount{ source.mVertexCount },
          mSkinned{ source.mSkinned } {
    }

    auto MeshRenderData::UpdatePositions( eastl::span<const core::float3> positions ) -> void {
        if ( positions.size() != mVertexCount || positions.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Deformed vertex count differs from the source mesh." );
        }

        // Accumulate triangle area normals before normalizing each render vertex.
        eastl::vector<core::float3> normals( positions.size(), core::float3{} );

        for ( core::usize i{}; i + 2 < mIndices.size(); i += 3 ) {
            const auto a{ mIndices[i] }, b{ mIndices[i + 1] }, c{ mIndices[i + 2] };
            const auto normal{ glm::cross( positions[b] - positions[a], positions[c] - positions[a] ) };
            normals[a] += normal;
            normals[b] += normal;
            normals[c] += normal;
        }

        // Publish positions, normals and bounds as one geometry revision.
        auto minimum{ positions.front() }, maximum{ minimum };

        for ( core::usize i{}; i < positions.size(); ++i ) {
            const auto normal{ glm::dot( normals[i], normals[i] ) > .00000001f ? glm::normalize( normals[i] ) : core::float3{ 0, 1, 0 } };
            std::memcpy( mAttributes.data() + mOffsets[static_cast<core::usize>( MeshStream::Position )] + i * sizeof( core::float3 ), &positions[i], sizeof( core::float3 ) );
            std::memcpy( mAttributes.data() + mOffsets[static_cast<core::usize>( MeshStream::Normal )] + i * sizeof( core::float3 ), &normal, sizeof( core::float3 ) );
            minimum = glm::min( minimum, positions[i] );
            maximum = glm::max( maximum, positions[i] );
        }

        mSortCenter = ( minimum + maximum ) * .5f;
        mBoundsExtent = ( maximum - minimum ) * .5f;
        BumpRevision();
    }

    MeshRenderData::MeshRenderData( eastl::span<const asset::VertexDescription_Std430Alignment> vertices,
        eastl::span<const core::u32> indices, bool skinned )
        : mSkinned{ skinned } {
        MKT_BEGIN_PROFILER();

        if ( vertices.empty() || indices.empty() || vertices.size() > eastl::numeric_limits<core::u32>::max() ||
             indices.size() > eastl::numeric_limits<core::u32>::max() ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid indexed mesh upload dimensions." );
        }

        mVertexCount = static_cast<core::u32>( vertices.size() );
        mIndices.reserve( indices.size() );

        // Indices cannot change after construction. Fingerprint their ordering while
        // copying them so temporal consumers never scan large meshes every frame.
        // UpdatePositions changes geometry revisions without changing correspondence.
        mTopologyHash = kMeshTopologyHashSeed;

        for ( const core::u32 index: indices ) {
            if ( index >= vertices.size() ) {
                MKT_THROW_RUNTIME_ERROR( "Mesh index exceeds its vertex stream." );
            }

            mIndices.push_back( index );
            mTopologyHash = ( mTopologyHash ^ index ) * kMeshTopologyHashPrime;
        }

        // float3 streams deliberately use 12-byte elements. Shaders load three
        // scalars, avoiding std430's 16-byte array stride for float3 pointers.
        constexpr eastl::array<core::usize, kMeshStreamCount> strides{ 12, 12, 16, 8, 8, 16, 16, 16, 16 };
        const core::usize streamCount{ skinned ? kMeshStreamCount : static_cast<core::usize>( MeshStream::Joints0 ) };
        core::usize byteCount{};

        for ( core::usize stream{}; stream < streamCount; ++stream ) {
            byteCount = ( byteCount + 15 ) & ~core::usize{ 15 };
            mOffsets[stream] = byteCount;

            if ( vertices.size() > ( eastl::numeric_limits<core::usize>::max() - byteCount - 15 ) / strides[stream] ) {
                MKT_THROW_RUNTIME_ERROR( "Mesh attribute allocation is too large." );
            }

            byteCount += vertices.size() * strides[stream];
        }

        mAttributes.resize( ( byteCount + 15 ) & ~core::usize{ 15 } );
        core::float3 minimum{ vertices.front().mPosition };
        core::float3 maximum{ minimum };

        for ( core::usize index{}; index < vertices.size(); ++index ) {
            const auto& vertex{ vertices[index] };
            const eastl::array<const void*, kMeshStreamCount> sources{
                &vertex.mPosition, &vertex.mNormals, &vertex.mColors, &vertex.mUv0, &vertex.mUv1,
                &vertex.mJoints0, &vertex.mWeights0, &vertex.mJoints1, &vertex.mWeights1
            };

            for ( core::usize stream{}; stream < streamCount; ++stream ) {
                std::memcpy( mAttributes.data() + mOffsets[stream] + index * strides[stream], sources[stream], strides[stream] );
            }

            minimum = glm::min( minimum, vertex.mPosition );
            maximum = glm::max( maximum, vertex.mPosition );
        }

        mSortCenter = minimum * 0.5f + maximum * 0.5f;
        mBoundsExtent = maximum * 0.5f - minimum * 0.5f;
    }

    auto MeshRenderData::GetAttributes() const -> eastl::span<const core::ubyte> {
        return { mAttributes.data(), mAttributes.size() };
    }

    auto MeshRenderData::GetIndices() const -> eastl::span<const core::u32> {
        return { mIndices.data(), mIndices.size() };
    }

    auto MeshRenderData::GetVertexCount() const -> core::u32 {
        return mVertexCount;
    }

    auto MeshRenderData::GetTopologyHash() const -> core::u64 {
        return mTopologyHash;
    }

    auto MeshRenderData::GetOffsets() const -> const eastl::array<core::u64, kMeshStreamCount>& {
        return mOffsets;
    }

    auto MeshRenderData::GetSortCenter() const -> core::float3 {
        return mSortCenter;
    }

    auto MeshRenderData::GetBoundsExtent() const -> core::float3 {
        return mBoundsExtent;
    }

    auto MeshRenderData::IsSkinned() const -> bool {
        return mSkinned;
    }
}// namespace mikoto::renderer
