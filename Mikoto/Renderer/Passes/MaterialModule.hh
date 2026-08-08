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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    // Displays a material on a sphere with a convoluted background on top of a cube
    // using a directional light with shadows
    class MaterialModule final {
    public:
        explicit MaterialModule(RenderResolution resolution);

        auto RegisterPasses(  FrameGraph &graph ) -> void;

    private:
        RenderResolution mResolution{ RenderResolution::e1080P };
    };
}

#endif // MIKOTO_MATERIAL_DEBUG_HH
