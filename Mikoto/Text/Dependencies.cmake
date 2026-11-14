#    Copyright 2026 ケイト
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(MIKOTO_USE_VCPKG)
    find_package(Freetype REQUIRED)
    find_package(harfbuzz CONFIG REQUIRED)

    # msdf-atlas-gen loads msdfgen through find_dependency. Its msdfgen version
    # recreates an alias on every lookup, so do not load that package twice.
    find_package(msdf-atlas-gen CONFIG REQUIRED)

    target_link_libraries(Mikoto PUBLIC harfbuzz::harfbuzz Freetype::Freetype msdfgen::msdfgen msdf-atlas-gen::msdf-atlas-gen)
else()
    # FreeType
    # Shaping uses HarfBuzz OpenType tables directly. Keep FreeType independent
    # to avoid a circular dependency between the two libraries.
    set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "Disable HarfBuzz requirement in FreeType" FORCE)
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz support in FreeType" FORCE)
    set(FT_WITH_HARFBUZZ OFF CACHE BOOL "Don't build with HarfBuzz" FORCE)
    set(FT_DISABLE_ZLIB OFF CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG OFF CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(FT2_BUILD_LIBRARY ON CACHE BOOL "" FORCE)
    set(FT_WITH_CMAKE ON CACHE BOOL "" FORCE)

    find_package(PNG REQUIRED)
    FetchContent_Declare(freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG VER-2-14-3
    )
    FetchContent_MakeAvailable(freetype)

    # Font loading needs msdfgen's FreeType bridge even when FreeType is bundled.
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()
    # SlugHorn also calls FindFreetype. Point it at this same build, avoiding
    # a second system FreeType with different headers or linkage.
    set(FREETYPE_INCLUDE_DIR_freetype2 "${freetype_SOURCE_DIR}/include")
    set(FREETYPE_INCLUDE_DIR_ft2build "${freetype_SOURCE_DIR}/include")
    set(FREETYPE_LIBRARY freetype)
    set(MSDFGEN_CORE_ONLY OFF)

    # msdfgen
    set(MSDFGEN_USE_VCPKG OFF)
    set(MSDFGEN_BUILD_STANDALONE OFF)
    set(MSDFGEN_DISABLE_SVG ON)
    set(MSDFGEN_USE_SKIA OFF)
    set(MSDFGEN_DISABLE_PNG ON)
    set(MSDFGEN_DYNAMIC_RUNTIME OFF)
    FetchContent_Declare(msdfgen
        GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
        GIT_TAG v1.13
    )
    FetchContent_MakeAvailable(msdfgen)

    # msdf-atlas-gen
    set(MSDF_ATLAS_BUILD_STANDALONE OFF)
    set(MSDF_ATLAS_USE_VCPKG OFF)
    set(MSDF_ATLAS_USE_SKIA OFF)
    set(MSDF_ATLAS_DYNAMIC_RUNTIME OFF)
    set(MSDF_ATLAS_MSDFGEN_EXTERNAL ON)
    set(MSDF_ATLAS_INSTALL OFF)
    FetchContent_Declare(msdf_atlas_gen
        GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
        GIT_TAG v1.4
    )
    FetchContent_MakeAvailable(msdf_atlas_gen)

    # HarfBuzz combines emoji modifiers, flags and supported ZWJ sequences before
    # FreeType rasterizes their colour glyph. No global parser/font state is shared.
    set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(harfbuzz
        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
        GIT_TAG 14.5.1
    )
    FetchContent_MakeAvailable(harfbuzz)

    target_link_libraries(Mikoto PUBLIC harfbuzz freetype msdfgen::msdfgen msdf-atlas-gen::msdf-atlas-gen)
endif()

# Pin the atlas/shader contract together. No rendering example or Python
# dependencies are needed: Mikoto owns the graphics backend and text layout.
set(SLUGHORN_FREETYPE ON CACHE BOOL "Build SlugHorn's font adapter" FORCE)
set(SLUGHORN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SLUGHORN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(slughorn
    GIT_REPOSITORY https://github.com/AlphaPixel/slughorn.git
    GIT_TAG v0.0.2
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(slughorn)
target_link_libraries(Mikoto PRIVATE slughorn)
