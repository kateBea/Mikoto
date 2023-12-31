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

// Third-Party Libraries
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/ranges.h"

// Project Headers
#include "Types.hh"

// Debug Break
#if defined(WIN32) || defined(WIN64) && !defined(NDEBUG)
    #include <intrin.h>
    #define MKT_DEBUG_BREAK() __debugbreak()
#endif

#define LINUX_ERROR_RETURN_CODE -1

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

#define MKT_PRINT_CONTAINER(CONTAINER)                                                                      \
    fmt::print("{}\n", CONTAINER)

#define MKT_PRINT_FORMATTED(...)                                                                            \
    fmt::print(__VA_ARGS__)

#define MKT_COLOR_PRINT_FORMATTED(COLOR, ...)                                                               \
    fmt::print(fmt::fg(COLOR), __VA_ARGS__)

#define MKT_COLOR_STYLE_PRINT_FORMATTED(COLOR, STYLE, ...)                                              \
    fmt::print(fmt::fg(COLOR) | STYLE, __VA_ARGS__)

#define MKT_BIND_EVENT_FUNC(function)                                                                       \
    [this]<typename... Args>(Args&&... args) -> decltype(auto) {                                            \
        return this->function(std::forward<Args>(args)...);                                                 \
    }

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
#define DELETE_COPY_FOR(CLASS_NAME)           \
    CLASS_NAME(const CLASS_NAME&)       = delete;   \
    auto operator=(const CLASS_NAME&)   = delete

/**
 * Disable MOVE constructor and operator for CLASS_NAME
 * */
#define DELETE_MOVE_FOR(CLASS_NAME)           \
    CLASS_NAME(CLASS_NAME&&)            = delete;   \
    auto operator=(CLASS_NAME&&)        = delete

/**
 * Output colors. They can be mixed by using a bit OR when used with
 * MKT_COLOR_STYLE_PRINT_FORMATTED or MKT_COLOR_PRINT_FORMATTED. E.g.:
 * MKT_FMT_COLOR_ALICE_BLUE | MKT_FMT_COLOR_ANTIQUE_WHITE
 * */
#define MKT_FMT_COLOR_ALICE_BLUE                 fmt::color::alice_blue
#define MKT_FMT_COLOR_ANTIQUE_WHITE              fmt::color::antique_white
#define MKT_FMT_COLOR_AQUA fmt::color::aqua
#define MKT_FMT_COLOR_AQUAMARINE                 fmt::color::aquamarine
#define MKT_FMT_COLOR_AZURE                      fmt::color::azure
#define MKT_FMT_COLOR_BEIGE                      fmt::color::beige
#define MKT_FMT_COLOR_BISQUE                     fmt::color::bisque
#define MKT_FMT_COLOR_BLACK                      fmt::color::black
#define MKT_FMT_COLOR_BLANCHED_ALMOND            fmt::color::blanched_almond
#define MKT_FMT_COLOR_BLUE                       fmt::color::blue
#define MKT_FMT_COLOR_BLUE_VIOLET                fmt::color::blue_violet
#define MKT_FMT_COLOR_BROWN                      fmt::color::brown
#define MKT_FMT_COLOR_BURLY_WOOD                 fmt::color::burly_wood
#define MKT_FMT_COLOR_CADET_BLUE                 fmt::color::cadet_blue
#define MKT_FMT_COLOR_CHARTREUSE                 fmt::color::chartreuse
#define MKT_FMT_COLOR_CHOCOLATE                  fmt::color::chocolate
#define MKT_FMT_COLOR_CORAL                      fmt::color::coral
#define MKT_FMT_COLOR_CORNFLOWER_BLUE            fmt::color::cornflower_blue
#define MKT_FMT_COLOR_CORNSILK                   fmt::color::cornsilk
#define MKT_FMT_COLOR_CRIMSON                    fmt::color::crimson
#define MKT_FMT_COLOR_CYAN                       fmt::color::cyan
#define MKT_FMT_COLOR_DARK_BLUE                  fmt::color::dark_blue
#define MKT_FMT_COLOR_DARK_CYAN                  fmt::color::dark_cyan
#define MKT_FMT_COLOR_DARK_GOLDEN_ROD            fmt::color::dark_golden_rod
#define MKT_FMT_COLOR_DARK_GRAY                  fmt::color::dark_gray
#define MKT_FMT_COLOR_DARK_GREEN                 fmt::color::dark_green
#define MKT_FMT_COLOR_DARK_KHAKI                 fmt::color::dark_khaki
#define MKT_FMT_COLOR_DARK_MAGENTA               fmt::color::dark_magenta
#define MKT_FMT_COLOR_DARK_OLIVE_GREEN           fmt::color::dark_olive_green
#define MKT_FMT_COLOR_DARK_ORANGE                fmt::color::dark_orange
#define MKT_FMT_COLOR_DARK_ORCHID                fmt::color::dark_orchid
#define MKT_FMT_COLOR_DARK_RED                   fmt::color::dark_red
#define MKT_FMT_COLOR_DARK_SALMON                fmt::color::dark_salmon
#define MKT_FMT_COLOR_DARK_SEA_GREEN             fmt::color::dark_sea_green
#define MKT_FMT_COLOR_DARK_SLATE_BLUE            fmt::color::dark_slate_blue
#define MKT_FMT_COLOR_DARK_SLATE_GRAY            fmt::color::dark_slate_gray
#define MKT_FMT_COLOR_DARK_TURQUOISE             fmt::color::dark_turquoise
#define MKT_FMT_COLOR_DARK_VIOLET                fmt::color::dark_violet
#define MKT_FMT_COLOR_DEEP_PINK                  fmt::color::deep_pink
#define MKT_FMT_COLOR_DEEP_SKY_BLUE              fmt::color::deep_sky_blue
#define MKT_FMT_COLOR_DIM_GRAY                   fmt::color::dim_gray
#define MKT_FMT_COLOR_DODGER_BLUE                fmt::color::dodger_blue
#define MKT_FMT_COLOR_FIRE_BRICK                 fmt::color::fire_brick
#define MKT_FMT_COLOR_FLORAL_WHITE               fmt::color::floral_white
#define MKT_FMT_COLOR_FOREST_GREEN               fmt::color::forest_green
#define MKT_FMT_COLOR_FUCHSIA                    fmt::color::fuchsia
#define MKT_FMT_COLOR_GAINSBORO                  fmt::color::gainsboro
#define MKT_FMT_COLOR_GHOST_WHITE                fmt::color::ghost_white
#define MKT_FMT_COLOR_GOLD                       fmt::color::gold
#define MKT_FMT_COLOR_GOLDEN_ROD                 fmt::color::golden_rod
#define MKT_FMT_COLOR_GRAY                       fmt::color::gray
#define MKT_FMT_COLOR_GREEN                      fmt::color::green
#define MKT_FMT_COLOR_GREEN_YELLOW               fmt::color::green_yellow
#define MKT_FMT_COLOR_HONEY_DEW                  fmt::color::honey_dew
#define MKT_FMT_COLOR_HOT_PINK                   fmt::color::hot_pink
#define MKT_FMT_COLOR_INDIAN_RED                 fmt::color::indian_red
#define MKT_FMT_COLOR_INDIGO                     fmt::color::indigo
#define MKT_FMT_COLOR_IVORY                      fmt::color::ivory
#define MKT_FMT_COLOR_KHAKI                      fmt::color::khaki
#define MKT_FMT_COLOR_LAVENDER                   fmt::color::lavender
#define MKT_FMT_COLOR_LAVENDER_BLUSH             fmt::color::lavender_blush
#define MKT_FMT_COLOR_LAWN_GREEN                 fmt::color::lawn_green
#define MKT_FMT_COLOR_LEMON_CHIFFON              fmt::color::lemon_chiffon
#define MKT_FMT_COLOR_LIGHT_BLUE                 fmt::color::light_blue
#define MKT_FMT_COLOR_LIGHT_CORAL                fmt::color::light_coral
#define MKT_FMT_COLOR_LIGHT_CYAN                 fmt::color::light_cyan
#define MKT_FMT_COLOR_LIGHT_GOLDEN_ROD_YELLOW    fmt::color::light_golden_rod_yellow
#define MKT_FMT_COLOR_LIGHT_GRAY                 fmt::color::light_gray
#define MKT_FMT_COLOR_LIGHT_GREEN                fmt::color::light_green
#define MKT_FMT_COLOR_LIGHT_PINK                 fmt::color::light_pink
#define MKT_FMT_COLOR_LIGHT_SALMON               fmt::color::light_salmon
#define MKT_FMT_COLOR_LIGHT_SEA_GREEN            fmt::color::light_sea_green
#define MKT_FMT_COLOR_LIGHT_SKY_BLUE             fmt::color::light_sky_blue
#define MKT_FMT_COLOR_LIGHT_SLATE_GRAY           fmt::color::light_slate_gray
#define MKT_FMT_COLOR_LIGHT_STEEL_BLUE           fmt::color::light_steel_blue
#define MKT_FMT_COLOR_LIGHT_YELLOW               fmt::color::light_yellow
#define MKT_FMT_COLOR_LIME                       fmt::color::lime
#define MKT_FMT_COLOR_LIME_GREEN                 fmt::color::lime_green
#define MKT_FMT_COLOR_LINEN                      fmt::color::linen
#define MKT_FMT_COLOR_MAGENTA                    fmt::color::magenta
#define MKT_FMT_COLOR_MAROON                     fmt::color::maroon
#define MKT_FMT_COLOR_MEDIUM_AQUA_MARINE         fmt::color::medium_aqua_marine
#define MKT_FMT_COLOR_MEDIUM_BLUE                fmt::color::medium_blue
#define MKT_FMT_COLOR_MEDIUM_ORCHID              fmt::color::medium_orchid
#define MKT_FMT_COLOR_MEDIUM_PURPLE              fmt::color::medium_purple
#define MKT_FMT_COLOR_MEDIUM_SEA_GREEN           fmt::color::medium_sea_green
#define MKT_FMT_COLOR_MEDIUM_SLATE_BLUE          fmt::color::medium_slate_blue
#define MKT_FMT_COLOR_MEDIUM_SPRING_GREEN        fmt::color::medium_spring_green
#define MKT_FMT_COLOR_MEDIUM_TURQUOISE           fmt::color::medium_turquoise
#define MKT_FMT_COLOR_MEDIUM_VIOLET_RED          fmt::color::medium_violet_red
#define MKT_FMT_COLOR_MIDNIGHT_BLUE              fmt::color::midnight_blue
#define MKT_FMT_COLOR_MINT_CREAM                 fmt::color::mint_cream
#define MKT_FMT_COLOR_MISTY_ROSE                 fmt::color::misty_rose
#define MKT_FMT_COLOR_MOCCASIN                   fmt::color::moccasin
#define MKT_FMT_COLOR_NAVAJO_WHITE               fmt::color::navajo_white
#define MKT_FMT_COLOR_NAVY                       fmt::color::navy
#define MKT_FMT_COLOR_OLD_LACE                   fmt::color::old_lace
#define MKT_FMT_COLOR_OLIVE                      fmt::color::olive
#define MKT_FMT_COLOR_OLIVE_DRAB                 fmt::color::olive_drab
#define MKT_FMT_COLOR_ORANGE                     fmt::color::orange
#define MKT_FMT_COLOR_ORANGE_RED                 fmt::color::orange_red
#define MKT_FMT_COLOR_ORCHID                     fmt::color::orchid
#define MKT_FMT_COLOR_PALE_GOLDEN_ROD            fmt::color::pale_golden_rod
#define MKT_FMT_COLOR_PALE_GREEN                 fmt::color::pale_green
#define MKT_FMT_COLOR_PALE_TURQUOISE             fmt::color::pale_turquoise
#define MKT_FMT_COLOR_PALE_VIOLET_RED            fmt::color::pale_violet_red
#define MKT_FMT_COLOR_PAPAYA_WHIP                fmt::color::papaya_whip
#define MKT_FMT_COLOR_PEACH_PUFF                 fmt::color::peach_puff
#define MKT_FMT_COLOR_PERU                       fmt::color::peru
#define MKT_FMT_COLOR_PINK                       fmt::color::pink
#define MKT_FMT_COLOR_PLUM                       fmt::color::plum
#define MKT_FMT_COLOR_POWDER_BLUE                fmt::color::powder_blue
#define MKT_FMT_COLOR_PURPLE                     fmt::color::purple
#define MKT_FMT_COLOR_REBECCA_PURPLE             fmt::color::rebecca_purple
#define MKT_FMT_COLOR_RED fmt::color::red
#define MKT_FMT_COLOR_ROSY_BROWN                 fmt::color::rosy_brown
#define MKT_FMT_COLOR_ROYAL_BLUE                 fmt::color::royal_blue
#define MKT_FMT_COLOR_SADDLE_BROWN               fmt::color::saddle_brown
#define MKT_FMT_COLOR_SALMON                     fmt::color::salmon
#define MKT_FMT_COLOR_SANDY_BROWN                fmt::color::sandy_brown
#define MKT_FMT_COLOR_SEA_GREEN                  fmt::color::sea_green
#define MKT_FMT_COLOR_SEA_SHELL                  fmt::color::sea_shell
#define MKT_FMT_COLOR_SIENNA                     fmt::color::sienna
#define MKT_FMT_COLOR_SILVER                     fmt::color::silver
#define MKT_FMT_COLOR_SKY_BLUE                   fmt::color::sky_blue
#define MKT_FMT_COLOR_SLATE_BLUE                 fmt::color::slate_blue
#define MKT_FMT_COLOR_SLATE_GRAY                 fmt::color::slate_gray
#define MKT_FMT_COLOR_SNOW                       fmt::color::snow
#define MKT_FMT_COLOR_SPRING_GREEN               fmt::color::spring_green
#define MKT_FMT_COLOR_STEEL_BLUE                 fmt::color::steel_blue
#define MKT_FMT_COLOR_TAN                        fmt::color::tan
#define MKT_FMT_COLOR_TEAL                       fmt::color::teal
#define MKT_FMT_COLOR_THISTLE                    fmt::color::thistle
#define MKT_FMT_COLOR_TOMATO                     fmt::color::tomato
#define MKT_FMT_COLOR_TURQUOISE                  fmt::color::turquoise
#define MKT_FMT_COLOR_VIOLET                     fmt::color::violet
#define MKT_FMT_COLOR_WHEAT                      fmt::color::wheat
#define MKT_FMT_COLOR_WHITE                      fmt::color::white
#define MKT_FMT_COLOR_WHITE_SMOKE                fmt::color::white_smoke
#define MKT_FMT_COLOR_YELLOW                     fmt::color::yellow
#define MKT_FMT_COLOR_YELLOW_GREEN               fmt::color::yellow_green

#define MKT_FMT_STYLE_UNDERLINE fmt::emphasis::underline
#define MKT_FMT_STYLE_BOLD fmt::emphasis::bold
#define MKT_FMT_STYLE_ITALIC fmt::emphasis::italic

namespace Mikoto {
    struct SystemInfo {
        Int64_T TotalRam;  // Total usable main memory size in kB
        Int64_T FreeRam;   // Available memory size in kB
        Int64_T SharedRam; // Amount of shared memory in kB
    };


    /**
     * @brief Make a a path to a char string
     *
     * Transforms wide char strings to byte char strings. On Windows std::filesystem::string
     * returns a string of wide char types (wchar_t), whereas on linux it returns a string of char
     * @param path path to the file
     * @returns string of byte sized characters
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
     * Prints a glm matrix to the standard output
     * @param mat matrix to be printed
     * @tparam GLMMatrixType matrix type
     * */
    template<typename GLMMatrixType>
    inline auto PrintMatrix(const GLMMatrixType& mat) -> void {
        UInt32_T rowIdx{};
        UInt32_T colIdx{};

        for ( ; rowIdx < mat.length(); ++rowIdx) {
            for ( ; colIdx < mat.length(); ++colIdx)
                MKT_PRINT_FORMATTED("{} ", mat[rowIdx][colIdx]);

            MKT_PRINT_FORMATTED("\n");
        }
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

        return std::string{ std::istreambuf_iterator<CharArray::value_type>(file), std::istreambuf_iterator<CharArray::value_type>() };
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

        auto parseLongFromLine{
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
