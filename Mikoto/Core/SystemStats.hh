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

#ifndef SYSTEMSTATS_HH
#define SYSTEMSTATS_HH

#if defined( _WIN32 )
#include <windows.h>
#endif

#include <mutex>

#include <Common/Common.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class SystemStats final : public Singleton<SystemStats> {
    public:
        // Can be called by background thread every x time
        // to avoid overhead spinning cpu
        auto Update() -> void;

        // Tells how often we fetch system stats (in seconds)
        auto SetUpdateFrequency( Int32 frequency ) -> void;

        // RAM in bytes
        MKT_NODISCARD auto GetSharedRam() const -> double;
        MKT_NODISCARD auto GetFreeRam() const -> double;
        MKT_NODISCARD auto GetTotalRam() const -> double;

        // CPU
        MKT_NODISCARD auto GetCpuUsage() const -> double;
        MKT_NODISCARD auto GetCpuName() const -> const std::string&;

        // VRAM (stub, later Vulkan/Allocator integration)
        MKT_NODISCARD auto GetVramUsage() const -> double;
        MKT_NODISCARD auto SetVramUsage( double usageBytes ) -> void;

        MKT_NODISCARD auto GetProcessRamUsage() const -> double;

    private:
        double m_SharedRam{ 0.0 };
        double m_FreeRam{ 0.0 };
        double m_TotalRam{ 0.0 };
        double m_CpuUsage{ 0.0 };
        double m_VramUsage{ 0.0 };

        double m_ProcessRamUsage{ 0.0 };

        std::string m_CpuName{};
        std::string m_GpuName{ "Unknown GPU" };

        std::mutex m_UpdateMutex{};
#if defined( _WIN32 )
        ULARGE_INTEGER m_LastIdleTime{};
        ULARGE_INTEGER m_LastKernelTime{};
        ULARGE_INTEGER m_LastUserTime{};
#endif

        Int32 m_UpdateFrequency{ 1 };
    };
}

#endif
