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

#include <Renderer/Rhi/CommandQueue.hh>

namespace mikoto::renderer::rhi {

    auto SubmitInfo::SetSignalFence( FenceHandle handle ) -> SubmitInfo & {
        mSinalFence = handle;
        return *this;
    }

    auto SubmitInfo::SetSignalValue( core::u64 signalValue ) -> SubmitInfo & {
        mSignalValue = signalValue;
        return *this;
    }

    auto SubmitInfo::AddCommandList( CommandListHandle cmd ) -> SubmitInfo & {
        mCommands.emplace_back( cmd );
        return *this;
    }
    auto IQueue::GetType() const -> QueueType {
        return mType;
    }

    auto IQueue::GetOpSupportFlags() const -> QueueOpSupportFlags {
        return mOpSupportFlags;
    }

    IQueue::IQueue( QueueType type, QueueOpSupportFlags flags )
        : mType{ type }, mOpSupportFlags{ flags }
    {}
}