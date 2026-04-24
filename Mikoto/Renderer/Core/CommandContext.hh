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

#ifndef MIKOTO_COMMAND_CONTEXT_HH
#define MIKOTO_COMMAND_CONTEXT_HH

#include <EASTL/utility.h>
#include <ankerl/unordered_dense.h>

#include <../../Memory/BufferSpan.hh>
#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Memory/Allocator.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FrameGraphNode.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Rhi.hh>

namespace mikoto::renderer {

    class CommandContext final {
    public:
        CommandContext( GpuDevice* device, FrameGraphNode* pass, FrameGraphResourceManager* resourceManager );

        auto BeginRender() -> void;
        auto EndRender() -> void;

        auto BindPipeline( ResourceID pipelineID ) -> void;
        auto Dispatch( u32 groupX, u32 groupY, u32 groupZ ) -> void;

        auto PushConstants( const auto& data ) -> void {
            mPushConstantsData->Push( data );
        }

    private:
        GpuDevice* mDevice{};
        FrameGraphNode* mNode{};
        FrameGraphResourceManager* mResourceManager{};

        BufferSpanHandle mPushConstantsData{};

        // Render state
        Rect mScissors{};
        Viewport mViewport{};
        Color mClearColor{};

        CommandListHandle mCommands{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_COMMAND_CONTEXT_HH