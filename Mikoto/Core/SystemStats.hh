//
// Created by kate on 10/27/25.
//

#ifndef SYSTEMSTATS_HH
#define SYSTEMSTATS_HH

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
        auto GetSharedRam() const -> double;
        auto GetFreeRam() const -> double;
        auto GetTotalRam() const -> double;

        // CPU
        auto GetCpuUsage() const -> double;
        auto GetCpuName() const -> const std::string&;

        // GPU (filled externally, e.g., RenderService)
        auto GetGpuName() const -> const std::string&;
        auto SetGpuName( const std::string& name ) -> void;

        // VRAM (stub, later Vulkan/Allocator integration)
        auto GetVramUsage() const -> double;
        auto SetVramUsage( double usageBytes ) -> void;

        auto GetProcessRamUsage() const -> double;

    private:
        double m_SharedRam{ 0.0 };
        double m_FreeRam{ 0.0 };
        double m_TotalRam{ 0.0 };
        double m_CpuUsage{ 0.0 };
        double m_VramUsage{ 0.0 };

        double m_ProcessRamUsage{ 0.0 };

        std::string m_CpuName{};
        std::string m_GpuName{ "Unknown GPU" };

        Int32 m_UpdateFrequency{ 1 };
    };
}



#endif //
