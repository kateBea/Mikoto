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

#ifndef MIKOTO_RENDERER_CORE_GPU_QUERY_PROFILER_HH
#define MIKOTO_RENDERER_CORE_GPU_QUERY_PROFILER_HH

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer {

    struct GpuPassTiming {
        eastl::string mName{};
        core::f64 mMilliseconds{};
    };

    struct GpuQuerySnapshot {
        core::u64 mSubmission{};
        core::f64 mFrameMilliseconds{};

        rhi::QueryResult mStatistics{};

        eastl::vector<GpuPassTiming> mPasses{};

        bool mHasTiming{};
    };

    // Measures graph submissions using only the RHI. A full ring drops samples
    // instead of waiting for the GPU, including when capture has been paused.
    class GpuQueryProfiler final {
    public:
        explicit GpuQueryProfiler( rhi::IGpuDevice* device );

        MKT_NODISCARD auto IsEnabled() const -> bool;
        MKT_NODISCARD auto GetDroppedSamples() const -> core::u64;
        MKT_NODISCARD auto GetError() const -> eastl::string_view;
        MKT_NODISCARD auto GetSnapshot() const -> const GpuQuerySnapshot&;

        auto SetEnabled( bool enabled ) -> void;

        auto EndPass( rhi::ICommandList& commands ) -> void;
        auto EndFrame( rhi::ICommandList& commands ) -> void;
        auto BeginFrame( rhi::ICommandList& commands, rhi::FenceHandle completion ) -> void;
        auto BeginPass( rhi::ICommandList& commands, eastl::string_view name ) -> void;

        auto Submitted( core::u64 value ) -> void;

    private:
        struct Slot {
            core::u64 mCompletion{};
            rhi::FenceHandle mFence{};

            rhi::QueryPoolHandle mTimestamps{};
            rhi::QueryPoolHandle mStatistics{};
            eastl::vector<eastl::string> mPassNames{};

            bool mPending{};
            bool mPassActive{};
        };

    private:
        auto Collect() -> void;
        auto InitializeSlot( Slot& slot ) -> void;

    private:
        static constexpr core::u32 kMaxPasses{ 1023 };

        Slot* mRecording{};
        rhi::IGpuDevice* mDevice{};

        core::u64 mDropped{};

        eastl::array<Slot, 4> mSlots{};

        eastl::string mError{};
        GpuQuerySnapshot mSnapshot{};

        bool mEnabled{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_GPU_QUERY_PROFILER_HH

