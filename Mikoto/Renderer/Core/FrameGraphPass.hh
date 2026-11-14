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

#ifndef MIKOTO_RENDERER_CORE_FRAME_GRAPH_PASS_HH
#define MIKOTO_RENDERER_CORE_FRAME_GRAPH_PASS_HH

#include <source_location>

#include <EASTL/string.h>
#include <EASTL/functional.h>
#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Blackboard.hh>

#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Core/FrameGraphTypes.hh>

namespace mikoto::renderer {
    class CommandContext;

    struct FrameGraphPass {
        eastl::string mName{};
        eastl::string mDebugLabel{};
        std::source_location mSourceLocation{};
        FGPassType mType{ FGPassType::eGraphics };
        rhi::CommandListHandle mCommandList{};

        bool mIsAlive{ true };

        FGExecutionPolicy mExecutionPolicy{ FGExecutionPolicy::ePerFrame };

        eastl::function<void()> mBuilderCallback{};
        eastl::function<void( CommandContext&, core::Blackboard& )> mExecuteCallback{};

        static constexpr core::u32 kMaxDependencies{ 32 };
        static constexpr core::u32 kMaxSuccessors{ 32 };
        static constexpr core::u32 kMaxResources{ 32 };

        eastl::fixed_vector<FGResourceHandle, kMaxResources> mReadResources{};
        eastl::fixed_vector<FGResourceHandle, kMaxResources> mWriteResources{};

        ankerl::unordered_dense::set<eastl::string> mDependsOn{};
        eastl::fixed_vector<eastl::string, kMaxSuccessors> mSuccessors{};

        ankerl::unordered_dense::map<FGResourceHandle, FGResourceTrack> mResourceStates{};

        core::u32 mInDegree{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_FRAME_GRAPH_PASS_HH
