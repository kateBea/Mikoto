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

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_git(OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/shader-slang/slang.git
    REF ca6e0a657c52881166295f11c14d57afdf0de481)
include("${CMAKE_CURRENT_LIST_DIR}/FixIncludeSourceLocation.cmake")
mikoto_fix_slang_include_source_location("${SOURCE_PATH}")
# git archives omit submodules. Fetch the exact gitlinks from v2026.17.1,
# using vcpkg's download cache rather than running git during configuration.
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/richgel999/miniz.git REF 6ef6c68f4fcbb8287aa8edf9c6670804932f41c6)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/miniz")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/lz4/lz4.git REF 7f71be01f2c6f6b2c3cddb67eef8c38eab12ffd4)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/lz4")
# slang-fiddle generates compiler sources and embeds Lua's implementation.
# It needs Slang's exact Lua checkout, even with standalone Slang tools disabled.
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/lua/lua.git REF 3fe7be956f23385aa1950dc31e2f25127ccfc0ea)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/lua")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/martinus/unordered_dense.git REF 73f3cbb237e84d483afafc743f1f14ec53e12314)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/unordered_dense")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/swiftlang/swift-cmark.git REF 924936d0427cb25a61169739a7660230bffa6ea6)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/cmark")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/fastfloat/fast_float.git REF e0b53eaf63c6d00e0725788ef1dbb759aa321d79)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/fast_float")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/microsoft/mimalloc.git REF 8c532c32c3c96e5ba1f2283e032f69ead8add00f)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/mimalloc")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/KhronosGroup/SPIRV-Headers.git REF 496543121ce6419f23d6fa5d7194ba66c36212d2)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/spirv-headers")
vcpkg_from_git(OUT_SOURCE_PATH DEP_SOURCE
    URL https://github.com/bkaradzic/metal-cpp.git REF a63bd172ddcba73a3d87ca32032b66ad41ddb9a6)
file(COPY "${DEP_SOURCE}/" DESTINATION "${SOURCE_PATH}/external/metal-cpp")
file(WRITE "${SOURCE_PATH}/cmake/slang_git_version" "v${VERSION}\n")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/MikotoRuntime.cmake" DESTINATION "${SOURCE_PATH}")
file(APPEND "${SOURCE_PATH}/CMakeLists.txt" "\ninclude(MikotoRuntime.cmake)\n")
set(MIKOTO_SLANG_OPTIONS "")
foreach(feature IN ITEMS DXIL GFX SLANGD SLANGC SLANGI SLANGRT SLANG_GLSLANG TESTS
    EXAMPLES RELEASE_LTO SLANG_RHI REPLAYER SLANG_PROXY CUDA OPTIX NVAPI AFTERMATH)
    list(APPEND MIKOTO_SLANG_OPTIONS "-DSLANG_ENABLE_${feature}=OFF")
endforeach()
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
    ${MIKOTO_SLANG_OPTIONS}
    # Keep the compiler and its bundled dependencies static. Otherwise
    # vcpkg's shared default builds miniz.dll outside the generator's DLL path.
    -DSLANG_LIB_TYPE=STATIC -DSLANG_ENABLE_MIMALLOC=OFF -DBUILD_SHARED_LIBS=OFF -DSLANG_EMBED_CORE_MODULE=ON
    -DSLANG_SLANG_LLVM_FLAVOR=DISABLE -DSLANG_EXCLUDE_TINT=ON -DSLANG_EXCLUDE_DAWN=ON
    -DSLANG_EMBED_CORE_MODULE_SOURCE=OFF -DSLANG_ENABLE_PCH=OFF
    -DSLANG_USE_SYSTEM_VULKAN_HEADERS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5)
vcpkg_cmake_install()
vcpkg_copy_tools(TOOL_NAMES slang AUTO_CLEAN)
if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_cmake_config_fixup(PACKAGE_NAME slang CONFIG_PATH cmake)
else()
    vcpkg_cmake_config_fixup(PACKAGE_NAME slang CONFIG_PATH lib/cmake/slang)
endif()
vcpkg_cmake_config_fixup(PACKAGE_NAME mikoto-slang-glslang CONFIG_PATH share/mikoto-slang-glslang)
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/MikotoSlangStatic.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/slang")
file(APPEND "${CURRENT_PACKAGES_DIR}/share/slang/slangConfig.cmake"
    "\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/MikotoSlangStatic.cmake\")\n")
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/doc")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(INSTALL "${SOURCE_PATH}/LICENSES/" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/LICENSES")

# The compiler is static; its explicitly loaded glslang plugin remains a DLL.
set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)
