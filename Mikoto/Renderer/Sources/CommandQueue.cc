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

#include <Core/Profiler.hh>

#include <Renderer/Rhi/CommandQueue.hh>

namespace mikoto::renderer::rhi {

    auto SubmitInfo::AddCommandList( CommandListHandle cmd ) -> SubmitInfo & {
        MKT_BEGIN_PROFILER();
        mCommands.emplace_back( cmd );
        return *this;
    }

    auto SubmitInfo::AddWait( FenceHandle fence, core::u64 value ) -> SubmitInfo & {
        MKT_BEGIN_PROFILER();
        mWaits.emplace_back( SignalInfo{
            .mSignalValue = value,
            .mSignalFence = eastl::move( fence ) } );

        return *this;
    }

    auto SubmitInfo::AddSignal( FenceHandle fence, core::u64 value ) -> SubmitInfo & {
        MKT_BEGIN_PROFILER();
        mSignals.emplace_back( SignalInfo{
            .mSignalValue = value,
            .mSignalFence = eastl::move( fence ) } );

        return *this;
    }

    auto SubmitInfo::AddCommandLists( eastl::span<CommandListHandle> commands ) -> SubmitInfo & {
        MKT_BEGIN_PROFILER();
        mCommands.insert( mCommands.end(), commands.begin(), commands.end() );
        return *this;
    }

    auto SubmitInfo::AddWaits( eastl::span<SignalInfo> signals ) -> SubmitInfo & {
        MKT_BEGIN_PROFILER();
        mWaits.insert( mWaits.end(), signals.begin(), signals.end() );
        return *this;
    }

    auto SubmitInfo::AddSignals( eastl::span<SignalInfo> signals ) -> SubmitInfo & {
        MKT_BEGIN_PROFILER();
        mSignals.insert( mSignals.end(), signals.begin(), signals.end() );
        return *this;
    }
}