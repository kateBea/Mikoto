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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_INDIRECT_LIGHTING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_INDIRECT_LIGHTING_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>

#include <Scene/Camera.hh>

namespace mikoto::renderer {

    struct IndirectLightingModuleInfo {

    };

    // Screen space stuff (Reflections, Global Illumination)
    // I will probably move SSAO
    class IndirectLightingModule final {
    public:

        explicit IndirectLightingModule( rhi::RenderResolution resolution );

        auto SetCamera( const scene::Camera* camera ) -> void;

    private:
        const scene::Camera* mCamera{ nullptr };
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };

}
#endif//MIKOTO_RENDERER_FORWARD_RENDERER_INDIRECT_LIGHTING_MODULE_HH
