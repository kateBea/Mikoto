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

#include <Platform/PlatformWin32.hh>

#include <mutex>
#include <sstream>

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Platform.hh>
#include <Core/SystemStats.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::core {

    using namespace mikoto::memory;

#if defined( _WIN32 ) && defined(_MSC_VER)

#include <Pdh.h>
#include <PdhMsg.h>
#include <Psapi.h>

    MKT_NODISCARD static auto ReadCpuName() -> eastl::string {
        // https://stackoverflow.com/questions/4443864/how-do-i-get-hardware-info-such-as-cpu-name-total-ram-etc-with-vb6
        i32 cpuInfo[4]{ -1 };

        // 0x40 + 1 because some strings are
        // not guaranteed to be null terminated
        char cpuBrandString[0x41]{};

        __cpuid( cpuInfo, 0x80000000 );

        unsigned int nExIds{ (unsigned int)cpuInfo[0] };

        if ( nExIds >= 0x80000004 ) {
            int cpuData[4];
            __cpuid( cpuData, 0x80000002 );
            std::memcpy( cpuBrandString, cpuData, MKT_SIZEOF( cpuData ) );

            __cpuid( cpuData, 0x80000003 );
            std::memcpy( cpuBrandString + 16, cpuData, MKT_SIZEOF( cpuData ) );

            __cpuid( cpuData, 0x80000004 );
            std::memcpy( cpuBrandString + 32, cpuData, MKT_SIZEOF( cpuData ) );
        }

        return { cpuBrandString };
    }

    static auto GetProcessMemoryUsage() -> double {
        PROCESS_MEMORY_COUNTERS pmc{};

        // GetProcessMemoryInfo is a macro
        if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) ) {
            return as<f64>( pmc.WorkingSetSize );
        }
        return 0.0;
    }

    static auto GetCpuUsageWindows( ULARGE_INTEGER& lastIdleTime,
                                    ULARGE_INTEGER& lastKernelTime,
                                    ULARGE_INTEGER& lastUserTime ) -> double {
        FILETIME idleTime, kernelTime, userTime;
        if ( !GetSystemTimes( &idleTime, &kernelTime, &userTime ) )
            return 0.0;

        ULARGE_INTEGER idle{}, kernel{}, user{};
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

    static auto ReadCpuName() -> eastl::string {
        std::ifstream cpuinfo( "/proc/cpuinfo" );
        std::string line{};
        while ( std::getline( cpuinfo, line ) ) {
            if ( line.rfind( "model name", 0 ) == 0 ) {
                auto pos = line.find( ':' );
                if ( pos != std::string::npos )
                    return line.substr( pos + 2 ).c_str();
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

    auto SystemStats::Update( float ) -> void {
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

        mTotalRam = memValues["MemTotal"];
        mFreeRam = memValues["MemFree"];
        mSharedRam = memValues["Shmem"];

        // Update CPU info
        if ( mCpuName.empty() )
            mCpuName = ReadCpuName();

        mCpuUsage = ReadCpuUsage();
        mProcessRamUsage = GetProcessMemoryUsage();
#endif

#if defined( _WIN32 ) && defined(_MSC_VER)
        MEMORYSTATUSEX memStatus{
            .dwLength = MKT_SIZEOF( memStatus ) };
        if ( GlobalMemoryStatusEx( MKT_ADDRESSOF( memStatus ) ) ) {
            mTotalRam = as<f64>( memStatus.ullTotalPhys );
            mFreeRam = as<f64>( memStatus.ullAvailPhys );
            mSharedRam = 0.0;// Windows does not expose shared memory like Linux
        }

        mCpuUsage = GetCpuUsageWindows( mLastIdleTime, mLastKernelTime, mLastUserTime );
        mProcessRamUsage = GetProcessMemoryUsage();
#endif
    }

    auto SystemStats::Initialize() -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing SystemStats" );

        // No need to do this every frame, central processing unit
        // expected to be the same the entire run
#if defined( _WIN32 ) && defined(_MSC_VER)
        if ( mCpuName.empty() ) {
            mCpuName = ReadCpuName();
        }
#endif

        mIsInitialized = true;
    }

    auto SystemStats::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down SystemStats" );
    }

    auto SystemStats::SetUpdateFrequency( i32 frequency ) -> void {
        mUpdateFrequency = frequency;
    }

    auto SystemStats::GetSharedRam() const -> f64 {
        return mSharedRam;
    }

    auto SystemStats::GetFreeRam() const -> f64 {
        return mFreeRam;
    }

    auto SystemStats::GetTotalRam() const -> f64 {
        return mTotalRam;
    }

    auto SystemStats::GetCpuName() const -> eastl::string_view {
        return mCpuName;
    }

    auto SystemStats::GetCpuUsage() const -> f64 {
        return mCpuUsage;
    }

    auto SystemStats::GetGpuRamUsage() const -> f64 {
        return mGpuRamUsage;
    }

    auto SystemStats::GetProcessRamUsage() const -> f64 {
        return mProcessRamUsage;
    }

}// namespace Mikoto