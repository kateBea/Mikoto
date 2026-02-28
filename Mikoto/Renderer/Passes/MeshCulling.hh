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

#include <vector>
#include <cstdint>
#include <vector>
#include <optional>

#include <glm/glm.hpp>

#include <Scene/Scene.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    struct GeometryAllocation {
        UInt64 VertexOffset{};
        UInt64 VertexSize{};
        UInt64 IndexOffset{};
        UInt64 IndexSize{};
    };

    class GeometryBufferAllocator {
    public:
        explicit GeometryBufferAllocator( UInt64 vertexBufferSize, UInt64 indexBufferSize );

        auto Free( const GeometryAllocation& alloc ) -> void;
        auto Allocate( UInt64 vertexBytes, UInt64 indexBytes ) -> std::optional<GeometryAllocation>;

    private:
        struct FreeRange {
            UInt64 Offset{};
            UInt64 Size{};
        };

        auto AllocateFrom( std::vector<FreeRange>& freeList, UInt64 size ) -> std::optional<UInt64>;

    private:
        std::vector<FreeRange> m_VertexFreeList{};
        std::vector<FreeRange> m_IndexFreeList{};
    };

    // This class will be used to integrate indirect drawing
    // It uses a First-Fit allocation strategy to manage buffers for mesh data.
    class GeometryManager {
    public:
        auto Initialize( GpuDevice* device ) -> void;

        auto FreeMeshData( const MeshNode* node ) -> void;

        // Uploads vertex + index data into global buffers.
        // Returns allocation info needed for indirect drawing.
        auto UploadMeshData( const MeshNode* node ) -> GeometryAllocation;

    private:
        GpuDevice* m_Device{};

        BufferHandle m_VertexBuffers{};
        BufferHandle m_IndexBuffers{};

        GeometryBufferAllocator m_Allocator{ MKT_MIBIBYTES( 512 ), MKT_MIBIBYTES( 512 ) };

        ankerl::unordered_dense::map<const MeshNode*, GeometryAllocation> m_Allocations{};
    };

    class MeshCulling final {
    public:
        auto SetScene( Scene* scene) -> void;
        auto SetCamera( const Camera* camera ) -> void;
        auto RegisterPasses( FrameGraph& graph, GpuDevice* device ) -> void;

        auto DrawInstances( CommandContext& context ) -> void;
        auto DrawInstancesIndirect( CommandContext& context ) -> void;

    private:
        auto SetupInstanceData( CommandContext& context ) -> void;

        auto RegisterScatteredWrites(FrameGraph &graph) -> void;
        auto RegisterMeshCullingPass(FrameGraph &graph) -> void;

    private:
        struct SkinningInfo {
            std::array<Mat4F, MAX_BONES_PER_MESH> BoneTransforms{};
        };

    private:
        Scene* m_Scene{};
        const Camera* m_Camera{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        GeometryManager m_GeometryManager{};

        Size m_ObjectUpdateCount{};

        std::vector<UInt32> m_MeshInfoIndices{};
        std::vector<MeshParameters> m_MeshInfo{};

        ankerl::unordered_dense::map<MeshNode*, GeometryAllocation> m_IndirectDrawInfo{};

        std::vector<SkinningInfo> m_SkinningInfo{};
        ankerl::unordered_dense::map<MeshNode*, Size> m_MeshDrawInstanceCount{};
        ankerl::unordered_dense::map<MeshNode*, DrawIndexedState> m_DrawIndexedState{};
        ankerl::unordered_dense::map<MeshNode*, std::vector<ShaderMaterialParams>> m_InstanceInfos{};

        // Refactor material upload
        std::vector<ShaderMaterial> m_ShaderMaterialList{};
        ankerl::unordered_dense::map<UInt32, ShaderMaterial> m_ShaderMaterialPerDrawObject{};
    };

}

#endif // MIKOTO_MESH_CULLING_HH
