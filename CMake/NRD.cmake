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

# Original NRD shaders are embedded by its build, independently of Slang.
# Keep quad intrinsics off: the integration only requires baseline Vulkan 1.2 compute.
if(MIKOTO_ENABLE_NRD)
    # Build the host utility in this tree for native builds. It inherits the selected
    # MSVC runtime/debug format instead of configuring a second, incompatible toolchain.
    if(NOT CMAKE_CROSSCOMPILING)
        set(SHADERMAKE_TOOL OFF CACHE BOOL "" FORCE)
    endif()
    set(NRD_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    set(NRD_NRI OFF CACHE BOOL "" FORCE)
    set(NRD_EMBEDS_SPIRV_SHADERS ON CACHE BOOL "" FORCE)
    set(NRD_EMBEDS_DXIL_SHADERS ${WIN32} CACHE BOOL "" FORCE)
    set(NRD_EMBEDS_DXBC_SHADERS OFF CACHE BOOL "" FORCE)
    set(NRD_SUPPORTS_QUAD_INTRINSICS OFF CACHE BOOL "" FORCE)
    set(NRD_SUPPORTS_HISTORY_CONFIDENCE ON CACHE BOOL "" FORCE)
    set(NRD_NORMAL_ENCODING 2 CACHE STRING "" FORCE)
    set(NRD_ROUGHNESS_ENCODING 1 CACHE STRING "" FORCE)
    FetchContent_Declare(mikoto_nrd
        URL https://codeload.github.com/NVIDIA-RTX/NRD/zip/5f8e1400331650cfd6d18bd2cd137274ced6c3f5
        URL_HASH SHA256=741e659d7b5e08eff6f050058a1b5217a3df4db04abcdc5e95f618ae6a84f7fa
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mikoto_nrd)
    target_link_libraries(Mikoto PRIVATE NRD)
    target_compile_definitions(Mikoto PRIVATE
        MIKOTO_NRD_SHADER_INCLUDE_DIR="${mikoto_nrd_SOURCE_DIR}/Shaders")
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_NRD=1)

else()
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_NRD=0)
endif()
