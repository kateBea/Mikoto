/**
 * Common.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_COMMON_HH
#define MIKOTO_COMMON_HH

// C++ Standard Libraries
#include <filesystem>
#include <fstream>
#include <string>
#include <array>

#include <fmt/format.h>

// Project Headers
#include <Library/Utility/Types.hh>
#include <Models/SystemData.hh>

#define MKT_NODISCARD [[nodiscard]]
#define MKT_UNUSED_FUNC [[maybe_unused]]
#define MKT_UNUSED_VAR [[maybe_unused]]

// Set bit specified by the argument
#define BIT_SET(N)              (1 << N)

// Stringify
#define MKT_STRINGIFY(x) #x

// Engine version
#define MKT_ENGINE_VERSION_MAJOR 1
#define MKT_ENGINE_VERSION_MINOR 0
#define MKT_ENGINE_VERSION_PATCH 0

#define MKT_THROW_RUNTIME_ERROR(MESSAGE)                                                                    \
    throw std::runtime_error(fmt::format("Message: {}\n@File: {}\n@Line: {}", MESSAGE, __FILE__, __LINE__))

/**
 * Disable copy constructor and operator, move constructor
 * and operator for CLASS_NAME
 * */
#define DISABLE_COPY_AND_MOVE_FOR(CLASS_NAME)       \
    CLASS_NAME(const CLASS_NAME&)       = delete;   \
    auto operator=(const CLASS_NAME&)   = delete;   \
    CLASS_NAME(CLASS_NAME&&)            = delete;   \
    auto operator=(CLASS_NAME&&)        = delete

/**
 * Disable COPY constructor and operator for CLASS_NAME
 * */
#define DELETE_COPY_FOR(CLASS_NAME)                 \
    CLASS_NAME(const CLASS_NAME&)       = delete;   \
    auto operator=(const CLASS_NAME&)   = delete

namespace Mikoto {

    /**
     * Fetches the name of the CPU in the current platform.
     * @returns The name of the CPU from the current platform.
     * */
    MKT_NODISCARD inline auto GetCPUName() -> std::string {
        std::ifstream cpuInfoFile{};
        std::string cpuName{ "Unknown CPU Name" };

#if __linux__
        const Path_T cpuInfoPath{ "/proc/cpuinfo" };

        cpuInfoFile.open(cpuInfoPath);

        if (cpuInfoFile.is_open()) {
            bool found{ false };

            for (std::string line{}; !found && std::getline(cpuInfoFile, line); ) {
                if (line.empty() || line.starts_with("model name")) {
                    cpuName = line.substr(line.find_first_of(':') + 1, line.size() - 1);
                    found = true;
                }
            }
        }
#elif WIN32
        // TODO:

#endif

        cpuInfoFile.close();

        return cpuName;
    }


    /**
     * Fetch current system information, such as RAM usage, etc.
     * @returns A model containing system resource usage information.
     * */
    MKT_NODISCARD inline auto GetSystemCurrentInfo() -> SystemInfo {
        SystemInfo result{};

        auto parseLongFromLine{
                [](const std::string& line) -> Int64_T {
                    std::stringstream ss{ line };
                    Int64_T parsedInteger{};

                    ss >> parsedInteger;

                    return parsedInteger;
                }
        };

#if __linux__
        const Path_T cpuInfoPath{ "/proc/meminfo" };

        Size_T index{};
        std::array<std::string, 3> data{};

        constexpr std::array<std::string_view, 3> tokens{
            "MemTotal", "MemFree", "MemAvailable"
        };

        if ( std::ifstream cpuInfoFile{ cpuInfoPath }; cpuInfoFile.is_open()) {
            std::string line{};
            bool endReached{ false };

            while (!endReached) {
                std::getline(cpuInfoFile, line);

                if (line.empty() || index == data.size()) {
                    endReached = true;
                } else if (line.starts_with(tokens[index])) {
                    data[index++] = line.substr(line.find_first_of(':') + 1, line.size() - 1);
                }
            }
        }

        result.TotalRam = parseLongFromLine(data[0]);
        result.FreeRam = parseLongFromLine(data[1]);
        result.SharedRam = parseLongFromLine(data[2]);
#endif

        return result;
    }
}

#endif // MIKOTO_COMMON_HH
