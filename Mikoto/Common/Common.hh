/**
 * Common.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_COMMON_HH
#define MIKOTO_COMMON_HH

// C++ Standard Libraries
#include <memory>
#include <functional>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>
#include <string>
#include <array>

// Project Headers
#include <Common/Types.hh>
#include <Common/Constants.hh>

#define MKT_NODISCARD [[nodiscard]]
#define MKT_UNUSED_FUNC [[maybe_unused]]
#define MKT_UNUSED_VAR [[maybe_unused]]

// Set bit specified by the argument
#define BIT_SET(N)              (1 << N)

// OpenGL version
#define MKT_OPENGL_VERSION_MAJOR 4
#define MKT_OPENGL_VERSION_MINOR 3

// Vulkan version
#define MKT_VULKAN_VERSION_VARIANT 0
#define MKT_VULKAN_VERSION_MAJOR 1
#define MKT_VULKAN_VERSION_MINOR 3
#define MKT_VULKAN_VERSION_PATCH 0

// Engine version
#define MKT_ENGINE_VERSION_MAJOR 1
#define MKT_ENGINE_VERSION_MINOR 0
#define MKT_ENGINE_VERSION_PATCH 0

#define MKT_COLOR_PRINT_FORMATTED(COLOR, ...)                                                               \
    fmt::print(fmt::fg(COLOR), __VA_ARGS__)

#define MKT_COLOR_STYLE_PRINT_FORMATTED(COLOR, STYLE, ...)                                              \
    fmt::print(fmt::fg(COLOR) | STYLE, __VA_ARGS__)

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
    struct SystemInfo {
        Int64_T TotalRam;  // Total usable main memory size in kB
        Int64_T FreeRam;   // Available memory size in kB
        Int64_T SharedRam; // Amount of shared memory in kB
    };


    /**
     * @brief Make a a path to a char string. Transforms wide char strings to byte
     * char strings. On Windows std::filesystem::string returns a string of wide
     * char types (wchar_t), whereas on linux it returns a string of char.
     * @param path Path to the file
     * @returns String of byte sized .characters.
     * */
    inline auto GetByteChar(const Path_T &path) -> std::string {
        std::string fileDir(4096, '\0');
#if defined(_WIN32) || defined(_WIN64)
        wcstombs_s(nullptr, fileDir.data(), fileDir.size(), path.c_str(), 4096);
#else
        std::copy(path.native().begin(), path.native().end(), fileDir.begin());
#endif
        return fileDir;
    }


    /**
     * Returns a string containing the data from a file
     * @param path path to the file
     * @returns contents of the file
     * */
    inline auto GetFileData(const Path_T& path) -> std::string {
        std::ifstream file{ path, std::ios::binary };

        if (!file.is_open()) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to open file [ {} ]!", path.string()));
        }

        return std::string{ std::istreambuf_iterator<std::vector<char>::value_type>(file),
                std::istreambuf_iterator<std::vector<char>::value_type>() };
    }


    /**
     *
     * */
    MKT_NODISCARD inline auto GetCPUName() -> std::string {
        std::string line{};
        std::string cpuName{ "Unknown" };

        std::ifstream cpuInfoFile{};

#if __linux__
        const Path_T cpuInfoPath{ "/proc/cpuinfo" };

        cpuInfoFile.open(cpuInfoPath);

        if (cpuInfoFile.is_open()) {
            bool found{ false };
            while (!found) {
                std::getline(cpuInfoFile, line);

                if (line.empty() || line.starts_with("model name")) {
                    cpuName = line.substr(line.find_first_of(':') + 1, line.size() - 1);
                    found = true;
                }
            }
        }
#endif

        cpuInfoFile.close();

        return cpuName;
    }


    /**
     *
     * */
    MKT_NODISCARD inline auto GetSystemCurrentInfo() -> SystemInfo {
        SystemInfo result{};

        MKT_UNUSED_VAR auto parseLongFromLine{
                [](const std::string& line) -> Int64_T {
                    std::stringstream ss{ line };
                    Int64_T result{};

                    ss >> result;

                    return result;
                }
        };

#if __linux__
        const Path_T cpuInfoPath{ "/proc/meminfo" };

        std::string line{};
        std::ifstream cpuInfoFile{ cpuInfoPath };

        static std::array<std::string, 3> data{};
        static constexpr std::array<std::string_view, 3> tokens{ "MemTotal", "MemFree", "MemAvailable" };
        std::array<std::string_view, 3>::size_type index{};

        if (cpuInfoFile.is_open()) {
            bool end{ false };
            while (!end) {
                std::getline(cpuInfoFile, line);

                if (line.empty() || index == data.size()) {
                    end = true;
                }
                else {
                    if (line.starts_with(tokens[index])) {
                        data[index++] = line.substr(line.find_first_of(':') + 1, line.size() - 1);
                    }
                }
            }
        }

        result.TotalRam = parseLongFromLine(data[0]);
        result.FreeRam = parseLongFromLine(data[1]);
        result.SharedRam = parseLongFromLine(data[2]);
#endif

        return result;
    }


    /**
     * @brief Determines the size of a file.
     * @param path Absolute or relative path to the file.
     * @returns Size in KB of the given file, -1 if the file is not valid (not a directory or does not exist).
     * */
     MKT_NODISCARD inline auto GetFileSize(const Path_T& path) -> Int64_T {
        std::ifstream file{ path };

        if (!file.is_open()) {
            return -1;
        }

        return ( Int64_T )std::distance( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() ) / 1'000;
     }
}

#endif // MIKOTO_COMMON_HH
