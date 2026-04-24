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

#ifndef MIKOTO_TEXT_RENDERING_HH
#define MIKOTO_TEXT_RENDERING_HH

#include <Renderer/Text/Font.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <string_view>

namespace mikoto::renderer {

    class TextRendering final {
    public:

        explicit TextRendering( RenderResolution resolution);

        auto SetScene(const Scene* scene) -> void;
        auto SetCamera(const Camera* camera) -> void;
        auto RegisterPasses(FrameGraph& graph, GpuDevice* device) -> void;

    private:
        auto RegisterSlugPass(FrameGraph& graph, GpuDevice* device) -> void;
        auto RegisterDebugPass(FrameGraph& graph, GpuDevice* device) -> void;

    private:

        RenderResolution m_Resolution{ RenderResolution::e1080P };
    };

}// namespace Mikoto

#endif//MIKOTO_TEXT_RENDERING_HH
