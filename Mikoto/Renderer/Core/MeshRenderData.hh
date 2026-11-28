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

#ifndef MIKOTO_RENDERER_CORE_MESH_RENDER_DATA_HH
#define MIKOTO_RENDERER_CORE_MESH_RENDER_DATA_HH

#include <EASTL/span.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::asset {
    struct VertexDescription_Std430Alignment;
}// namespace mikoto::asset

namespace mikoto::renderer {

    // Order shared with GeometryAllocation in Material_Helpers.slang.
    enum class MeshStream : core::u32 {
        Position = 0,
        Normal = 1,
        Color = 2,
        UV0 = 3,
        UV1 = 4,
        Joints0 = 5,
        Weights0 = 6,
        Joints1 = 7,
        Weights1 = 8,
        Count = 9
    };
    inline constexpr core::usize kMeshStreamCount{ static_cast<core::usize>( MeshStream::Count ) };

    // Immutable upload source shared by instances and renderers. Attribute arrays
    // are packed once at import, rather than deinterleaved on the render thread.
    // Retaining CPU data allows a renderer/device to rebuild its arena without
    // reading device-local mesh buffers back to the CPU.
    class MeshRenderData final : public core::ReferenceCounted<MeshRenderData>, public core::RevisionTracked {
    public:
        MeshRenderData( eastl::span<const asset::VertexDescription_Std430Alignment> vertices,
                        eastl::span<const core::u32> indices, bool skinned );
        /** Private runtime copy for deformable instances; imported mesh sources stay shared. */
        explicit MeshRenderData( const MeshRenderData& source );
        /** Publish local-space soft-body positions and rebuild area-weighted normals/bounds. */
        auto UpdatePositions( eastl::span<const core::float3> positions ) -> void;
        ~MeshRenderData() override = default;

        MKT_NODISCARD auto GetAttributes() const -> eastl::span<const core::ubyte>;
        MKT_NODISCARD auto GetIndices() const -> eastl::span<const core::u32>;
        MKT_NODISCARD auto GetVertexCount() const -> core::u32;
        // Immutable index ordering, computed once and shared by runtime deformation copies.
        MKT_NODISCARD auto GetTopologyHash() const -> core::u64;
        MKT_NODISCARD auto GetOffsets() const -> const eastl::array<core::u64, kMeshStreamCount> &;
        MKT_NODISCARD auto GetSortCenter() const -> core::float3;
        MKT_NODISCARD auto GetBoundsExtent() const -> core::float3;
        MKT_NODISCARD auto IsSkinned() const -> bool;

    private:
        eastl::vector<core::u32> mIndices{};
        eastl::vector<core::ubyte> mAttributes{};
        eastl::array<core::u64, kMeshStreamCount> mOffsets{};

        core::float3 mSortCenter{};
        core::float3 mBoundsExtent{};

        core::u64 mTopologyHash{};
        core::u32 mVertexCount{};
        bool mSkinned{};
    };
    using MeshRenderDataHandle = core::RefConst<MeshRenderData>;
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_MESH_RENDER_DATA_HH
