//    Copyright 2025 ケイト
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

#ifndef MIKOTO_DEBUG_RENDERER_HH
#define MIKOTO_DEBUG_RENDERER_HH

#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace Mikoto {

    // Will be used to draw collider boxes, light radius, etc
    class DebugRenderer final : public Renderer {
    public:
        explicit DebugRenderer(GpuDevice* device);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Render( Scene * ) -> void override;

    private:
        GpuDevice* m_Device{};
        Unique<FrameGraph> m_FrameGraph{};
    };

}


#endif //MIKOTO_DEBUG_RENDERER_HH