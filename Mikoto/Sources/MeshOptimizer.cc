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

#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <meshoptimizer.h>

#include <Assets/Importer.hh>
#include <Assets/MeshOptimizer.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::asset {

    using namespace mikoto::core;

    namespace {
        /**
         * @brief Checks whether every mesh index references an existing vertex.
         *
         * @param mesh Mesh whose index range will be validated.
         * @returns True when all indices can safely be consumed by meshoptimizer.
         */
        auto HasValidIndices( const MeshNodeDescription& mesh ) -> bool {
            for ( const u32 index : mesh.mIndices ) {
                if ( index >= mesh.mVertices.size() ) {
                    return false;
                }
            }

            return true;
        }
    }

    auto OptimizeMesh( MeshNodeDescription& mesh ) -> void {
        if ( mesh.mVertices.empty() || mesh.mIndices.empty() ) {
            return;
        }

        // The cache optimizer reorders triangle indices. Do not guess the
        // topology for imported meshes that do not meet that contract.
        if ( mesh.mIndices.size() % 3 != 0 ) {
            MKT_CORE_LOGGER_WARN(
                "Skipping mesh optimization for '{}': its {} indices do not form a triangle list.",
                mesh.mName,
                mesh.mIndices.size() );
            return;
        }

        if ( !HasValidIndices( mesh ) ) {
            MKT_CORE_LOGGER_WARN( "Skipping mesh optimization for '{}': it contains an out-of-range index.", mesh.mName );
            return;
        }

        auto& vertices{ mesh.mVertices };
        auto& indices{ mesh.mIndices };

        // First index the existing vertex stream. This merges only complete
        // binary-identical vertices, preserving seams and every vertex
        // attribute used by the renderer.
        eastl::vector<u32> remap( vertices.size() );
        const usize indexedVertexCount{ meshopt_generateVertexRemap(
            remap.data(),
            indices.data(),
            indices.size(),
            vertices.data(),
            vertices.size(),
            sizeof( VertexDescription_Std430Alignment ) ) };

        eastl::vector<VertexDescription_Std430Alignment> indexedVertices( indexedVertexCount );
        eastl::vector<u32> indexedIndices( indices.size() );
        meshopt_remapIndexBuffer( indexedIndices.data(), indices.data(), indices.size(), remap.data() );
        meshopt_remapVertexBuffer(
            indexedVertices.data(),
            vertices.data(),
            vertices.size(),
            sizeof( VertexDescription_Std430Alignment ),
            remap.data() );

        vertices = eastl::move( indexedVertices );
        indices = eastl::move( indexedIndices );

        // The basic meshoptimizer pipeline: establish the final triangle order
        // for post-transform cache reuse, then optimize vertex memory access.
        meshopt_optimizeVertexCache( indices.data(), indices.data(), indices.size(), vertices.size() );

        // Vertex fetch optimization rewrites the indices to point to the new
        // vertex order. A separate destination keeps the operation explicit
        // and lets us discard vertices that are not referenced by any triangle.
        eastl::vector<VertexDescription_Std430Alignment> optimizedVertices( vertices.size() );
        const usize optimizedVertexCount{ meshopt_optimizeVertexFetch(
            optimizedVertices.data(),
            indices.data(),
            indices.size(),
            vertices.data(),
            vertices.size(),
            sizeof( VertexDescription_Std430Alignment ) ) };

        optimizedVertices.resize( optimizedVertexCount );
        vertices = eastl::move( optimizedVertices );
    }
}
