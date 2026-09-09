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

#ifndef MIKOTO_MESHOPTIMIZER_HH
#define MIKOTO_MESHOPTIMIZER_HH

namespace mikoto::asset {

    struct MeshNodeDescription;

    /**
     * @brief Optimizes an imported indexed triangle mesh for GPU rendering.
     *
     * Removes binary-identical vertices, then reorders the index and vertex
     * buffers for vertex-cache and vertex-fetch locality. The vertex
     * attributes and triangles themselves are preserved.
     *
     * @param mesh Imported mesh data to optimize in place before GPU buffers
     * are created.
     */
    auto OptimizeMesh( MeshNodeDescription& mesh ) -> void;
}

#endif//MIKOTO_MESHOPTIMIZER_HH
