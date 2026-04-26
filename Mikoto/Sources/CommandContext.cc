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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::renderer {

    CommandContext::CommandContext( GpuDevice* device, FrameGraphNode* pass, FrameGraphResourceManager* resourceManager )
        : mDevice{ device }, mNode{ pass }, mResourceManager{ resourceManager } {
        MKT_ASSERT( mNode, "Frame graph node cannot be null" );
        MKT_ASSERT( mDevice, "Gpu Device cannot be null" );
        MKT_ASSERT( mResourceManager, "Resource manager cannot be null" );

        QueueType type{ QueueType::eGraphics };
        switch (pass->mType) {
            case FrameGraphNodeType::eGraphics:
                type = QueueType::eGraphics;
                break;
            case FrameGraphNodeType::eCompute:
                type = QueueType::eCompute;
                break;
            case FrameGraphNodeType::eTransfer:
                type = QueueType::eTransfer;
                break;
            case FrameGraphNodeType::eGeneric:
                break;
        }

        if (pass->mType != FrameGraphNodeType::eGeneric) {
            mCommands = mDevice->CreateCommandList( type );
        }
    }

    auto CommandContext::BeginRender() -> void {

    }

    auto CommandContext::EndRender() -> void {

    }

    auto CommandContext::GetIndex( ResourceID resourceID ) -> u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FrameGraphResource pipeline{ mResourceManager->Get( resourceID ) };

        // Create gpu descriptor ID handle

        return 0; // TODO
    }

    auto CommandContext::BindPipeline( PipelineID pipelineID ) -> void {

    }

    auto CommandContext::Draw( u32 instanceCount ) -> void {

    }

    auto CommandContext::Dispatch( u32 groupX, u32 groupY, u32 groupZ ) -> void {
        mCommands->Dispatch( groupX, groupY, groupZ );
    }

    auto CommandContext::CopyBuffer( BufferID src, BufferID dest ) -> void {

    }
}// namespace mikoto::renderer