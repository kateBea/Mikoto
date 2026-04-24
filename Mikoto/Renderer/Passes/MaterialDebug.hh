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

#ifndef MIKOTO_MATERIAL_DEBUG_HH
#define MIKOTO_MATERIAL_DEBUG_HH

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace mikoto {
    // Displays a material on a sphere with a convoluted background on top of a cube
    // using a directional light with shadows
    class MaterialDebug final {
    public:
        explicit MaterialDebug(RenderResolution resolution);

        auto RegisterPasses(  FrameGraph &graph ) -> void;

    private:
        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };
    };
}

#endif // MIKOTO_MATERIAL_DEBUG_HH
