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

#ifndef MIKOTO_POST_EFFECTS_PASSES_HH
#define MIKOTO_POST_EFFECTS_PASSES_HH

#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>
#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Assets/Font.hh>

#include <Assets//Texture.hh>

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    // These will register pass callbacks and their execute methods
    auto RegisterTextRender( FrameGraph& pass ) -> void;

}// namespace Mikoto

#endif // MIKOTO_POST_EFFECTS_PASSES_HH

