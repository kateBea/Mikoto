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

#ifndef MIKOTO_BARRIER_HH
#define MIKOTO_BARRIER_HH

#include <string>
#include <Renderer/Core/FrameGraphStructures.hh>

namespace Mikoto {
    struct ResourceBarrierInfo {
        std::string Name{};

        FrameResourceType Type{ FrameResourceType::INVALID };

        FramePassNodeType PreviousPass{ FramePassNodeType::GENERIC };
        FramePassNodeType NextPass{ FramePassNodeType::GENERIC };

        FrameResourceState PreState{ FrameResourceState::Undefined };
        FrameResourceState PostState{ FrameResourceState::Undefined };
    };
}
#endif//MIKOTO_BARRIER_HH
