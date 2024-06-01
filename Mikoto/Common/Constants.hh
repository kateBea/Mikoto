/**
 * Constants.hh
 * Created by kate on 9/9/23.
 * */

#ifndef MIKOTO_CONSTANTS_HH
#define MIKOTO_CONSTANTS_HH

// Third-Party Libraries
#include "glm/glm.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/ranges.h"

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
    constexpr glm::vec3 GLM_UNIT_VECTOR_X{ 1.0f, 0.0f, 0.0f };
    constexpr glm::vec3 GLM_UNIT_VECTOR_Y{ 0.0f, 1.0f, 0.0f };
    constexpr glm::vec3 GLM_UNIT_VECTOR_Z{ 0.0f, 0.0f, 1.0f };
    constexpr glm::mat4 GLM_IDENTITY_MAT4{ glm::mat4(1.0) };

}

#endif // MIKOTO_CONSTANTS_HH
