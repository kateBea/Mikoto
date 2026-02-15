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

#include <vector>
#include <iostream>

#include <meshoptimizer.h>

#include <Assets/MeshOptimizer.hh>

namespace Mikoto {

    auto MeshOptimizer::OptimizerTestRun() -> void {
        // Dummy mesh: a single quad (two triangles)
        std::vector<float> vertices{
            -1.f, -1.f, 0.f,
             1.f, -1.f, 0.f,
             1.f,  1.f, 0.f,
            -1.f,  1.f, 0.f
        };

        std::vector<unsigned int> indices{
            0, 1, 2,
            2, 3, 0
        };

        std::cout << "Original triangle count: "
                  << indices.size() / 3 << "\n";

        // --- Cache optimization ---
        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size() / 3);

        // --- Overdraw optimization ---
        meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(),
                                 (const float*)vertices.data(), vertices.size() / 3, sizeof(float) * 3, 1.05f);

        // --- Vertex fetch remap ---
        std::vector<unsigned int> remap(indices.size());
        size_t vertexCount = meshopt_generateVertexRemap(
            remap.data(),
            indices.data(),
            indices.size(),
            vertices.data(),
            vertices.size() / 3,
            sizeof(float) * 3
        );

        std::vector<unsigned int> newIndices(indices.size());
        std::vector<float> newVertices(vertexCount * 3);

        meshopt_remapIndexBuffer(newIndices.data(), indices.data(), indices.size(), remap.data());
        meshopt_remapVertexBuffer(newVertices.data(), vertices.data(), vertices.size() / 3, sizeof(float) * 3, remap.data());

        // --- Vertex fetch optimization ---
        meshopt_optimizeVertexFetch(newVertices.data(), newIndices.data(), newIndices.size(),
                                    newVertices.data(), newVertices.size() / 3, sizeof(float) * 3);

        std::cout << "Optimized vertex count: " << vertexCount << "\n";
        std::cout << "Optimization complete.\n";

    }
}