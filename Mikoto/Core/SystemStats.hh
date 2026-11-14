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

#ifndef MIKOTO_CORE_SYSTEM_STATS_HH
#define MIKOTO_CORE_SYSTEM_STATS_HH

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Platform/Windows/Windows.hh>

namespace mikoto::core {

    class SystemStats final : public ISubsystem, public Singleton<SystemStats> {
    public:
        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // Can be called by background thread every x time
        // to avoid overhead spinning cpu
        auto Update( f32 ts ) -> void override;

        // Tells how often we fetch system stats (in seconds)
        auto SetUpdateFrequency( core::f32 seconds ) -> void;

        // CPU
        MKT_NODISCARD auto GetCpuUsage() const -> f64;
        MKT_NODISCARD auto GetCpuName() const -> eastl::string_view;
        MKT_NODISCARD auto GetProcessRamUsage() const -> f64;

        // In bytes
        MKT_NODISCARD auto GetSharedRam() const -> f64;
        MKT_NODISCARD auto GetFreeRam() const -> f64;
        MKT_NODISCARD auto GetTotalRam() const -> f64;


    private:
        f64 mSharedRam{ 0.0 };
        f64 mFreeRam{ 0.0 };
        f64 mTotalRam{ 0.0 };
        f64 mCpuUsage{ 0.0 };

        f64 mProcessRamUsage{ 0.0 };

        eastl::string mCpuName{};
        eastl::string mGpuName{ "Unknown GPU" };

#if defined( _WIN32 )
        ULARGE_INTEGER mLastIdleTime{};
        ULARGE_INTEGER mLastKernelTime{};
        ULARGE_INTEGER mLastUserTime{};
#endif

        f32 mUpdateFrequency{ 0.5f };
        f32 mElapsedSinceUpdate{};
    };
}// namespace mikoto::core

#endif// MIKOTO_CORE_SYSTEM_STATS_HH
