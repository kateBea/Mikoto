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

#ifndef MIKOTO_DEBUG_PASSES_HH
#define MIKOTO_DEBUG_PASSES_HH

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    class DebugPasses {
    public:
        explicit DebugPasses( RenderResolution resolution );

        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto RegisterTrianglePass( FrameGraph& graph ) -> void;
        auto RegisterTexturePass( FrameGraph& graph ) -> void;
        auto RegisterSimpleComputePass( FrameGraph& graph ) -> void;

    private:
        RenderResolution mResolution{ RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_DEBUG_PASSES_HH