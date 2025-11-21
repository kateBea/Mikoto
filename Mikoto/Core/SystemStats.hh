//
// Created by kate on 10/27/25.
//

#ifndef SYSTEMSTATS_HH
#define SYSTEMSTATS_HH

#if defined( _WIN32 )
#include <windows.h>
#endif

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

#if defined( _WIN32 )
        ULARGE_INTEGER m_LastIdleTime{};
        ULARGE_INTEGER m_LastKernelTime{};
        ULARGE_INTEGER m_LastUserTime{};
#endif

        Int32 m_UpdateFrequency{ 1 };
    };
}

#endif
