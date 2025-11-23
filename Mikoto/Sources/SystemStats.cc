//
// Created by kate on 10/27/25.
//

#include <map>
#include <sstream>
#include <string>

#include <Core/SystemStats.hh>
#include <Renderer/Core/RenderService.hh>

namespace Mikoto {

    static inline SystemStats g_SystemStats{};

#if defined( _WIN32 )
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <windows.h>

    static auto ReadCpuName() -> std::string {
        int cpuInfo[4] = { -1 };
        char cpuBrandString[0x40];
        __cpuid( cpuInfo, 0x80000000 );
        unsigned int nExIds = cpuInfo[0];
        memset( cpuBrandString, 0, sizeof( cpuBrandString ) );

        if ( nExIds >= 0x80000004 ) {
            int cpuData[4];
            __cpuid( cpuData, 0x80000002 );
            memcpy( cpuBrandString, cpuData, sizeof( cpuData ) );
            __cpuid( cpuData, 0x80000003 );
            memcpy( cpuBrandString + 16, cpuData, sizeof( cpuData ) );
            __cpuid( cpuData, 0x80000004 );
            memcpy( cpuBrandString + 32, cpuData, sizeof( cpuData ) );
        }
        return std::string( cpuBrandString );
    }

    static auto GetProcessMemoryUsage() -> double {
        PROCESS_MEMORY_COUNTERS pmc;
        if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) ) {
            return static_cast<double>( pmc.WorkingSetSize );
        }
        return 0.0;
    }

    static auto GetCpuUsageWindows( ULARGE_INTEGER& lastIdleTime,
                                    ULARGE_INTEGER& lastKernelTime,
                                    ULARGE_INTEGER& lastUserTime ) -> double {
        FILETIME idleTime, kernelTime, userTime;
        if ( !GetSystemTimes( &idleTime, &kernelTime, &userTime ) )
            return 0.0;

        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;

        ULONGLONG sys = ( kernel.QuadPart + user.QuadPart ) - ( lastKernelTime.QuadPart + lastUserTime.QuadPart );
        ULONGLONG idleDiff = idle.QuadPart - lastIdleTime.QuadPart;

        lastIdleTime = idle;
        lastKernelTime = kernel;
        lastUserTime = user;

        if ( sys == 0 ) return 0.0;
        return 100.0 * double( sys - idleDiff ) / double( sys );
    }
#endif

#if defined( __linux__ )

    static auto ReadCpuName() -> std::string {
        std::ifstream cpuinfo( "/proc/cpuinfo" );
        std::string line;
        while ( std::getline( cpuinfo, line ) ) {
            if ( line.rfind( "model name", 0 ) == 0 ) {
                auto pos = line.find( ':' );
                if ( pos != std::string::npos )
                    return line.substr( pos + 2 );
            }
        }
        return "Unknown CPU";
    }

    // Helper to read CPU usage (percentage)
    static auto ReadCpuUsage() -> double {
        static std::array<unsigned long long, 7> last{};
        std::array<unsigned long long, 7> current{};

        std::ifstream stat( "/proc/stat" );
        if ( !( stat >> std::ws ).good() ) return 0.0;

        std::string cpu;
        stat >> cpu;
        for ( auto& v: current ) stat >> v;

        unsigned long long idle = current[3];
        unsigned long long total = 0;
        for ( auto v: current ) total += v;

        unsigned long long lastIdle = last[3];
        unsigned long long lastTotal = 0;
        for ( auto v: last ) lastTotal += v;

        unsigned long long deltaTotal = total - lastTotal;
        unsigned long long deltaIdle = idle - lastIdle;

        last = current;

        if ( deltaTotal == 0 ) return 0.0;
        return 100.0 * ( 1.0 - double( deltaIdle ) / double( deltaTotal ) );
    }

    static auto GetProcessMemoryUsage() -> double {
        std::ifstream statm( "/proc/self/statm" );
        if ( !statm.is_open() ) return 0.0;

        long rssPages = 0;
        statm >> rssPages;// first field is total program size (ignore)
        statm >> rssPages;// second field is resident set size (RSS)

        long pageSize = sysconf( _SC_PAGESIZE );// in bytes
        return static_cast<double>( rssPages * pageSize );
    }

#endif

    auto SystemStats::Update() -> void {

        m_VramUsage = RenderService::Get()->GetGpuDevice()->GetMemoryUsage();

#if defined( __linux__ )
        std::ifstream meminfo{ "/proc/meminfo" };
        if ( !meminfo.is_open() ) {
            return;
        }

        std::unordered_map<std::string, double> memValues{};
        std::string line{};

        while ( std::getline( meminfo, line ) ) {
            std::istringstream iss{ line };
            std::string key{};
            double value{};

            std::string unit{};

            if ( !( iss >> key >> value >> unit ) )
                continue;

            // Remove the trailing colon from keys like "MemTotal:"
            if ( !key.empty() && key.back() == ':' )
                key.pop_back();

            // Convert from kB to bytes
            memValues[key] = value * 1024.0;
        }

        m_TotalRam = memValues["MemTotal"];
        m_FreeRam = memValues["MemFree"];
        m_SharedRam = memValues["Shmem"];

        // Update CPU info
        if ( m_CpuName.empty() )
            m_CpuName = ReadCpuName();

        m_CpuUsage = ReadCpuUsage();
        m_ProcessRamUsage = GetProcessMemoryUsage();
#endif

#if defined( _WIN32 )
        MEMORYSTATUSEX memStatus{};
        memStatus.dwLength = sizeof( memStatus );
        if ( GlobalMemoryStatusEx( &memStatus ) ) {
            m_TotalRam = static_cast<double>( memStatus.ullTotalPhys );
            m_FreeRam = static_cast<double>( memStatus.ullAvailPhys );
            m_SharedRam = 0.0;// Windows does not expose shared memory like Linux
        }

        if ( m_CpuName.empty() ) m_CpuName = ReadCpuName();
        m_CpuUsage = GetCpuUsageWindows( m_LastIdleTime, m_LastKernelTime, m_LastUserTime );
        m_ProcessRamUsage = GetProcessMemoryUsage();
#endif
    }

    auto SystemStats::GetSharedRam() const -> double { return m_SharedRam; }
    auto SystemStats::GetFreeRam() const -> double { return m_FreeRam; }
    auto SystemStats::GetTotalRam() const -> double { return m_TotalRam; }
    auto SystemStats::GetCpuName() const -> const std::string& { return m_CpuName; }
    auto SystemStats::GetCpuUsage() const -> double { return m_CpuUsage; }
    auto SystemStats::GetVramUsage() const -> double { return m_VramUsage; }
    auto SystemStats::SetVramUsage( double usageBytes ) -> void { m_VramUsage = usageBytes; }
    auto SystemStats::GetProcessRamUsage() const -> double {
        return m_ProcessRamUsage;
    }

}// namespace Mikoto