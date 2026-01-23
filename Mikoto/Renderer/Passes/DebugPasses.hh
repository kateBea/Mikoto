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

#ifndef MIKOTO_DEBUG_PASSES_HH
#define MIKOTO_DEBUG_PASSES_HH

#include <string_view>

#include <Scene/Scene.hh>

#include <Library/Utility/Types.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    // These will register pass callbacks and their execute methods
    auto RegisterObjectOutline( FrameGraph& graph ) -> void;
    auto RegisterWireFrame( FrameGraph& graph ) -> void;
    auto RegisterMaterialPreview( FrameGraph& graph ) -> void;
    auto RegisterHelloTriangle( FrameGraph& graph ) -> void;
    auto RegisterSimpleCompute( FrameGraph& graph ) -> void;
    auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;
    auto RegisterHelloCube( FrameGraph& graph ) -> void;
    auto RegisterHelloTexture( FrameGraph& graph ) -> void;
}

#endif //MIKOTO_DEBUG_PASSES_HH