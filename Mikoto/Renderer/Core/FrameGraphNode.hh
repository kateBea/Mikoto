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

#ifndef MIKOTO_FRAME_GRAPH_NODE_HH
#define MIKOTO_FRAME_GRAPH_NODE_HH

#include <EASTL/string.h>
#include <EASTL/functional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Blackboard.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    class CommandContext;

    struct FrameGraphNode {
        eastl::string mName{};
        eastl::function<void( CommandContext &, Blackboard & )> mCallback{};
    };

}// namespace mikoto::renderer

#endif//MIKOTO_FRAME_GRAPH_NODE_HH
