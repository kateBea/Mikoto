//
// Created by kate on 10/27/25.
//

#include <string>
#include <map>
#include <sstream>
#include <sstream>

#include <Core/SystemStats.hh>

namespace Mikoto {

    static inline SystemStats g_SystemStats{};

#if defined(__linux__)

    static auto ReadCpuName() -> std::string {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos)
                    return line.substr(pos + 2);
            }
        }
        return "Unknown CPU";
    }

    // Helper to read CPU usage (percentage)
    static auto ReadCpuUsage() -> double {
        static std::array<unsigned long long, 7> last{};
        std::array<unsigned long long, 7> current{};

        std::ifstream stat("/proc/stat");
        if (!(stat >> std::ws).good()) return 0.0;

        std::string cpu;
        stat >> cpu;
        for (auto& v : current) stat >> v;

        unsigned long long idle  = current[3];
        unsigned long long total = 0;
        for (auto v : current) total += v;

        unsigned long long lastIdle = last[3];
        unsigned long long lastTotal = 0;
        for (auto v : last) lastTotal += v;

        unsigned long long deltaTotal = total - lastTotal;
        unsigned long long deltaIdle  = idle - lastIdle;

        last = current;

        if (deltaTotal == 0) return 0.0;
        return 100.0 * (1.0 - double(deltaIdle) / double(deltaTotal));
    }

#endif

    auto SystemStats::Update() -> void {

#if defined(__linux__)
        std::ifstream meminfo{ "/proc/meminfo" };
        if (!meminfo.is_open()) {
            return;
        }

        std::unordered_map<std::string, double> memValues{};
        std::string line{};

        while (std::getline(meminfo, line)) {
            std::istringstream iss{ line };
            std::string key{};
            double value{};

            std::string unit{};

            if (!(iss >> key >> value >> unit))
                continue;

            // Remove the trailing colon from keys like "MemTotal:"
            if (!key.empty() && key.back() == ':')
                key.pop_back();

            // Convert from kB to bytes
            memValues[key] = value * 1024.0;
        }

        m_TotalRam  = memValues["MemTotal"];
        m_FreeRam   = memValues["MemFree"];
        m_SharedRam = memValues["Shmem"];

        // Update CPU info
        if (m_CpuName.empty())
            m_CpuName = ReadCpuName();

        m_CpuUsage = ReadCpuUsage();
#endif

    }

    auto SystemStats::GetSharedRam() const -> double { return m_SharedRam; }
    auto SystemStats::GetFreeRam() const -> double { return m_FreeRam; }
    auto SystemStats::GetTotalRam() const -> double { return m_TotalRam; }
    auto SystemStats::GetCpuName() const -> const std::string& { return m_CpuName; }
    auto SystemStats::GetCpuUsage() const -> double { return m_CpuUsage; }
    auto SystemStats::GetGpuName() const -> const std::string& { return m_GpuName; }
    auto SystemStats::SetGpuName(const std::string& name) -> void { m_GpuName = name; }
    auto SystemStats::GetVramUsage() const -> double { return m_VramUsage; }
    auto SystemStats::SetVramUsage(double usageBytes) -> void { m_VramUsage = usageBytes; }

}// namespace Mikoto