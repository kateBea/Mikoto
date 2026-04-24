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

#ifndef MIKOTO_SYSTEM_STATS_HH
#define MIKOTO_SYSTEM_STATS_HH

#include <EASTL/string.h>
#if defined( _WIN32 )
#include <windows.h>
#endif

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>

namespace mikoto::core {

    class SystemStats final : public ISubsystem, public Singleton<SystemStats> {
    public:
        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // Can be called by background thread every x time
        // to avoid overhead spinning cpu
        auto Update(float ts) -> void override;

        // Tells how often we fetch system stats (in seconds)
        auto SetUpdateFrequency( i32 frequency ) -> void;

        // CPU
        MKT_NODISCARD auto GetCpuUsage() const -> f64;
        MKT_NODISCARD auto GetCpuName() const -> eastl::string_view;
        MKT_NODISCARD auto GetProcessRamUsage() const -> f64;
        // In bytes
        MKT_NODISCARD auto GetSharedRam() const -> f64;
        MKT_NODISCARD auto GetFreeRam() const -> f64;
        MKT_NODISCARD auto GetTotalRam() const -> f64;

        // GPU
        MKT_NODISCARD auto GetGpuRamUsage() const -> f64;


    private:
        f64 mSharedRam{ 0.0 };
        f64 mFreeRam{ 0.0 };
        f64 mTotalRam{ 0.0 };
        f64 mCpuUsage{ 0.0 };
        f64 mGpuRamUsage{ 0.0 };

        f64 mProcessRamUsage{ 0.0 };

        eastl::string mCpuName{};
        eastl::string mGpuName{ "Unknown GPU" };

#if defined( _WIN32 )
        ULARGE_INTEGER mLastIdleTime{};
        ULARGE_INTEGER mLastKernelTime{};
        ULARGE_INTEGER mLastUserTime{};
#endif

        i32 mUpdateFrequency{ 1 };
    };
}

#endif // MIKOTO_SYSTEM_STATS_HH
