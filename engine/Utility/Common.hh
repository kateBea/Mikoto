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

// Third-Party Libraries
#include <GL/glew.h>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

/*************************************************************
* MACROS -----------------------------------------------------
* ********************************************************+ */

// Debug Break
#if defined(WIN32) || defined(WIN64) && !defined(NDEBUG)
    #include <intrin.h>
    #define MKT_DEBUG_BREAK() __debugbreak()
#endif

// Set bit specified by the argument
#define BIT_SET(N)              (1 << N)

// OpenGL version
#define KT_OPENGL_VERSION_MAJOR    4
#define KT_OPENGL_VERSION_MINOR    3

#define KT_VULKAN_VERSION_VARIANT  0
#define KT_VULKAN_VERSION_MAJOR    1
#define KT_VULKAN_VERSION_MINOR    3
#define KT_VULKAN_VERSION_PATCH    0

/**
 * Follow up there is a list of macros to simplify formatted output.
 * They use fmt to print to the standard output a formatted string. The fmt syntax is:
 *
 * function("my_string replacement_field", args, ...)
 *
 * replacement_field ::=  "{" [arg_id] [":" (format_spec | chrono_format_spec)] "}"
 *
 * arg_id            ::=  integer | identifier
 * integer           ::=  digit+
 * digit             ::=  "0"..."9"
 * identifier        ::=  id_start id_continue*
 * id_start          ::=  "a"..."z" | "A"..."Z" | "_"
 * id_continue       ::=  id_start | digit
 *
 *
 * For more check out: https://fmt.dev/latest/syntax.html
 * */

/**
 * Output colors. They can be mixed by using a bit OR when used with
 * KT_COLOR_STYLE_PRINT_FORMATTED or KT_COLOR_PRINT_FORMATTED. E.g.:
 * KT_FMT_COLOR_ALICE_BLUE | KT_FMT_COLOR_ANTIQUE_WHITE
 * */
#define KT_FMT_COLOR_ALICE_BLUE                 fmt::color::alice_blue
#define KT_FMT_COLOR_ANTIQUE_WHITE              fmt::color::antique_white
#define KT_FMT_COLOR_AQUA                       fmt::color::aqua
#define KT_FMT_COLOR_AQUAMARINE                 fmt::color::aquamarine
#define KT_FMT_COLOR_AZURE                      fmt::color::azure
#define KT_FMT_COLOR_BEIGE                      fmt::color::beige
#define KT_FMT_COLOR_BISQUE                     fmt::color::bisque
#define KT_FMT_COLOR_BLACK                      fmt::color::black
#define KT_FMT_COLOR_BLANCHED_ALMOND            fmt::color::blanched_almond
#define KT_FMT_COLOR_BLUE                       fmt::color::blue
#define KT_FMT_COLOR_BLUE_VIOLET                fmt::color::blue_violet
#define KT_FMT_COLOR_BROWN                      fmt::color::brown
#define KT_FMT_COLOR_BURLY_WOOD                 fmt::color::burly_wood
#define KT_FMT_COLOR_CADET_BLUE                 fmt::color::cadet_blue
#define KT_FMT_COLOR_CHARTREUSE                 fmt::color::chartreuse
#define KT_FMT_COLOR_CHOCOLATE                  fmt::color::chocolate
#define KT_FMT_COLOR_CORAL                      fmt::color::coral
#define KT_FMT_COLOR_CORNFLOWER_BLUE            fmt::color::cornflower_blue
#define KT_FMT_COLOR_CORNSILK                   fmt::color::cornsilk
#define KT_FMT_COLOR_CRIMSON                    fmt::color::crimson
#define KT_FMT_COLOR_CYAN                       fmt::color::cyan
#define KT_FMT_COLOR_DARK_BLUE                  fmt::color::dark_blue
#define KT_FMT_COLOR_DARK_CYAN                  fmt::color::dark_cyan
#define KT_FMT_COLOR_DARK_GOLDEN_ROD            fmt::color::dark_golden_rod
#define KT_FMT_COLOR_DARK_GRAY                  fmt::color::dark_gray
#define KT_FMT_COLOR_DARK_GREEN                 fmt::color::dark_green
#define KT_FMT_COLOR_DARK_KHAKI                 fmt::color::dark_khaki
#define KT_FMT_COLOR_DARK_MAGENTA               fmt::color::dark_magenta
#define KT_FMT_COLOR_DARK_OLIVE_GREEN           fmt::color::dark_olive_green
#define KT_FMT_COLOR_DARK_ORANGE                fmt::color::dark_orange
#define KT_FMT_COLOR_DARK_ORCHID                fmt::color::dark_orchid
#define KT_FMT_COLOR_DARK_RED                   fmt::color::dark_red
#define KT_FMT_COLOR_DARK_SALMON                fmt::color::dark_salmon
#define KT_FMT_COLOR_DARK_SEA_GREEN             fmt::color::dark_sea_green
#define KT_FMT_COLOR_DARK_SLATE_BLUE            fmt::color::dark_slate_blue
#define KT_FMT_COLOR_DARK_SLATE_GRAY            fmt::color::dark_slate_gray
#define KT_FMT_COLOR_DARK_TURQUOISE             fmt::color::dark_turquoise
#define KT_FMT_COLOR_DARK_VIOLET                fmt::color::dark_violet
#define KT_FMT_COLOR_DEEP_PINK                  fmt::color::deep_pink
#define KT_FMT_COLOR_DEEP_SKY_BLUE              fmt::color::deep_sky_blue
#define KT_FMT_COLOR_DIM_GRAY                   fmt::color::dim_gray
#define KT_FMT_COLOR_DODGER_BLUE                fmt::color::dodger_blue
#define KT_FMT_COLOR_FIRE_BRICK                 fmt::color::fire_brick
#define KT_FMT_COLOR_FLORAL_WHITE               fmt::color::floral_white
#define KT_FMT_COLOR_FOREST_GREEN               fmt::color::forest_green
#define KT_FMT_COLOR_FUCHSIA                    fmt::color::fuchsia
#define KT_FMT_COLOR_GAINSBORO                  fmt::color::gainsboro
#define KT_FMT_COLOR_GHOST_WHITE                fmt::color::ghost_white
#define KT_FMT_COLOR_GOLD                       fmt::color::gold
#define KT_FMT_COLOR_GOLDEN_ROD                 fmt::color::golden_rod
#define KT_FMT_COLOR_GRAY                       fmt::color::gray
#define KT_FMT_COLOR_GREEN                      fmt::color::green
#define KT_FMT_COLOR_GREEN_YELLOW               fmt::color::green_yellow
#define KT_FMT_COLOR_HONEY_DEW                  fmt::color::honey_dew
#define KT_FMT_COLOR_HOT_PINK                   fmt::color::hot_pink
#define KT_FMT_COLOR_INDIAN_RED                 fmt::color::indian_red
#define KT_FMT_COLOR_INDIGO                     fmt::color::indigo
#define KT_FMT_COLOR_IVORY                      fmt::color::ivory
#define KT_FMT_COLOR_KHAKI                      fmt::color::khaki
#define KT_FMT_COLOR_LAVENDER                   fmt::color::lavender
#define KT_FMT_COLOR_LAVENDER_BLUSH             fmt::color::lavender_blush
#define KT_FMT_COLOR_LAWN_GREEN                 fmt::color::lawn_green
#define KT_FMT_COLOR_LEMON_CHIFFON              fmt::color::lemon_chiffon
#define KT_FMT_COLOR_LIGHT_BLUE                 fmt::color::light_blue
#define KT_FMT_COLOR_LIGHT_CORAL                fmt::color::light_coral
#define KT_FMT_COLOR_LIGHT_CYAN                 fmt::color::light_cyan
#define KT_FMT_COLOR_LIGHT_GOLDEN_ROD_YELLOW    fmt::color::light_golden_rod_yellow
#define KT_FMT_COLOR_LIGHT_GRAY                 fmt::color::light_gray
#define KT_FMT_COLOR_LIGHT_GREEN                fmt::color::light_green
#define KT_FMT_COLOR_LIGHT_PINK                 fmt::color::light_pink
#define KT_FMT_COLOR_LIGHT_SALMON               fmt::color::light_salmon
#define KT_FMT_COLOR_LIGHT_SEA_GREEN            fmt::color::light_sea_green
#define KT_FMT_COLOR_LIGHT_SKY_BLUE             fmt::color::light_sky_blue
#define KT_FMT_COLOR_LIGHT_SLATE_GRAY           fmt::color::light_slate_gray
#define KT_FMT_COLOR_LIGHT_STEEL_BLUE           fmt::color::light_steel_blue
#define KT_FMT_COLOR_LIGHT_YELLOW               fmt::color::light_yellow
#define KT_FMT_COLOR_LIME                       fmt::color::lime
#define KT_FMT_COLOR_LIME_GREEN                 fmt::color::lime_green
#define KT_FMT_COLOR_LINEN                      fmt::color::linen
#define KT_FMT_COLOR_MAGENTA                    fmt::color::magenta
#define KT_FMT_COLOR_MAROON                     fmt::color::maroon
#define KT_FMT_COLOR_MEDIUM_AQUA_MARINE         fmt::color::medium_aqua_marine
#define KT_FMT_COLOR_MEDIUM_BLUE                fmt::color::medium_blue
#define KT_FMT_COLOR_MEDIUM_ORCHID              fmt::color::medium_orchid
#define KT_FMT_COLOR_MEDIUM_PURPLE              fmt::color::medium_purple
#define KT_FMT_COLOR_MEDIUM_SEA_GREEN           fmt::color::medium_sea_green
#define KT_FMT_COLOR_MEDIUM_SLATE_BLUE          fmt::color::medium_slate_blue
#define KT_FMT_COLOR_MEDIUM_SPRING_GREEN        fmt::color::medium_spring_green
#define KT_FMT_COLOR_MEDIUM_TURQUOISE           fmt::color::medium_turquoise
#define KT_FMT_COLOR_MEDIUM_VIOLET_RED          fmt::color::medium_violet_red
#define KT_FMT_COLOR_MIDNIGHT_BLUE              fmt::color::midnight_blue
#define KT_FMT_COLOR_MINT_CREAM                 fmt::color::mint_cream
#define KT_FMT_COLOR_MISTY_ROSE                 fmt::color::misty_rose
#define KT_FMT_COLOR_MOCCASIN                   fmt::color::moccasin
#define KT_FMT_COLOR_NAVAJO_WHITE               fmt::color::navajo_white
#define KT_FMT_COLOR_NAVY                       fmt::color::navy
#define KT_FMT_COLOR_OLD_LACE                   fmt::color::old_lace
#define KT_FMT_COLOR_OLIVE                      fmt::color::olive
#define KT_FMT_COLOR_OLIVE_DRAB                 fmt::color::olive_drab
#define KT_FMT_COLOR_ORANGE                     fmt::color::orange
#define KT_FMT_COLOR_ORANGE_RED                 fmt::color::orange_red
#define KT_FMT_COLOR_ORCHID                     fmt::color::orchid
#define KT_FMT_COLOR_PALE_GOLDEN_ROD            fmt::color::pale_golden_rod
#define KT_FMT_COLOR_PALE_GREEN                 fmt::color::pale_green
#define KT_FMT_COLOR_PALE_TURQUOISE             fmt::color::pale_turquoise
#define KT_FMT_COLOR_PALE_VIOLET_RED            fmt::color::pale_violet_red
#define KT_FMT_COLOR_PAPAYA_WHIP                fmt::color::papaya_whip
#define KT_FMT_COLOR_PEACH_PUFF                 fmt::color::peach_puff
#define KT_FMT_COLOR_PERU                       fmt::color::peru
#define KT_FMT_COLOR_PINK                       fmt::color::pink
#define KT_FMT_COLOR_PLUM                       fmt::color::plum
#define KT_FMT_COLOR_POWDER_BLUE                fmt::color::powder_blue
#define KT_FMT_COLOR_PURPLE                     fmt::color::purple
#define KT_FMT_COLOR_REBECCA_PURPLE             fmt::color::rebecca_purple
#define KT_FMT_COLOR_RED                        fmt::color::red
#define KT_FMT_COLOR_ROSY_BROWN                 fmt::color::rosy_brown
#define KT_FMT_COLOR_ROYAL_BLUE                 fmt::color::royal_blue
#define KT_FMT_COLOR_SADDLE_BROWN               fmt::color::saddle_brown
#define KT_FMT_COLOR_SALMON                     fmt::color::salmon
#define KT_FMT_COLOR_SANDY_BROWN                fmt::color::sandy_brown
#define KT_FMT_COLOR_SEA_GREEN                  fmt::color::sea_green
#define KT_FMT_COLOR_SEA_SHELL                  fmt::color::sea_shell
#define KT_FMT_COLOR_SIENNA                     fmt::color::sienna
#define KT_FMT_COLOR_SILVER                     fmt::color::silver
#define KT_FMT_COLOR_SKY_BLUE                   fmt::color::sky_blue
#define KT_FMT_COLOR_SLATE_BLUE                 fmt::color::slate_blue
#define KT_FMT_COLOR_SLATE_GRAY                 fmt::color::slate_gray
#define KT_FMT_COLOR_SNOW                       fmt::color::snow
#define KT_FMT_COLOR_SPRING_GREEN               fmt::color::spring_green
#define KT_FMT_COLOR_STEEL_BLUE                 fmt::color::steel_blue
#define KT_FMT_COLOR_TAN                        fmt::color::tan
#define KT_FMT_COLOR_TEAL                       fmt::color::teal
#define KT_FMT_COLOR_THISTLE                    fmt::color::thistle
#define KT_FMT_COLOR_TOMATO                     fmt::color::tomato
#define KT_FMT_COLOR_TURQUOISE                  fmt::color::turquoise
#define KT_FMT_COLOR_VIOLET                     fmt::color::violet
#define KT_FMT_COLOR_WHEAT                      fmt::color::wheat
#define KT_FMT_COLOR_WHITE                      fmt::color::white
#define KT_FMT_COLOR_WHITE_SMOKE                fmt::color::white_smoke
#define KT_FMT_COLOR_YELLOW                     fmt::color::yellow
#define KT_FMT_COLOR_YELLOW_GREEN               fmt::color::yellow_green

#define  KT_NODISCARD [[nodiscard]]
/**
 * Output style
 * */
#define KT_FMT_STYLE_UNDERLINE fmt::emphasis::underline
#define KT_FMT_STYLE_BOLD fmt::emphasis::bold
#define KT_FMT_STYLE_ITALIC fmt::emphasis::italic

#define KT_PRINT_CONTAINER(__CONTAINER)                     \
    fmt::print("{}\n", __CONTAINER)

#define KT_PRINT_FORMATTED(...)                             \
    fmt::print(__VA_ARGS__)

#define KT_COLOR_PRINT_FORMATTED(__COLOR, ...)              \
    fmt::print(fmt::fg(__COLOR), __VA_ARGS__)

#define KT_COLOR_STYLE_PRINT_FORMATTED(__COLOR, __STYLE, ...)     \
    fmt::print(fmt::fg(__COLOR) | __STYLE, __VA_ARGS__)

#define KT_BIND_EVENT_FUNC(function) \
    [this](auto&&... args) -> decltype(auto) { \
        return this->function(std::forward<decltype(args)>(args)...); \
    }


/*************************************************************
* TYPE ALIAS -------------------------------------------------
* ********************************************************+ */

namespace Mikoto {
    using Path_T = std::filesystem::path;

    using Int8_T = std::int8_t;
    using Int16_T = std::int16_t;
    using Int32_T = std::int32_t;
    using Int64_T = std::int64_t;

    using UInt8_T = std::uint8_t;
    using UInt16_T = std::uint16_t;
    using UInt32_T = std::uint32_t;
    using UInt64_T = std::uint64_t;

    using UShort_T = unsigned short;
    using ULong_T = unsigned short;
    using ULongLong_T = unsigned long long;

    using Short_T = unsigned short;
    using Long_T = unsigned long;
    using LongLong_T = long long;

    using Size_T = std::size_t;

    using CharArray = std::vector<char>;

    /**
     * Transforms wide char strings to byte char strings. On Windows
     * std::filesystem::string returns a string of wide char types (wchar_t),
     * whereas on windows it returns a string of  char
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

    // TODO: temporary, need to specify args in reversed order at the moment to get desired output
    template<typename T, typename... Args>
    inline auto ConcatStr(const T& first, Args... args) -> std::string {
        std::string result{};

        if constexpr (sizeof...(args))
            result = std::move(ConcatStr(args...));

        result.append(fmt::to_string(first));

        return result;
    }

    template<typename GLMMatrixType>
    inline auto PrintMatrix(const GLMMatrixType& mat) -> void {
        UInt32_T rowIdx{};
        UInt32_T colIdx{};

        for ( ; rowIdx < mat.length(); ++rowIdx) {
            for ( ; colIdx < mat.length(); ++colIdx)
                KT_PRINT_FORMATTED("{} ", mat[rowIdx][colIdx]);

            KT_PRINT_FORMATTED("\n");
        }
    }

    inline auto GetFileData(const Path_T& path) -> CharArray {
        std::ifstream file{ path, std::ios::binary };

        if (!file.is_open())
            throw std::runtime_error("Failed to open SPR-V file");

        return CharArray{ std::istreambuf_iterator<CharArray::value_type>(file), std::istreambuf_iterator<CharArray::value_type>() };
    }
}

#endif // MIKOTO_COMMON_HH
