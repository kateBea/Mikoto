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

# Based on the vcpkg port. Keep upstream sources intact and explicitly
# match Protobuf linkage: otherwise GNS defines PROTOBUF_USE_DLLS on Windows
# even when the triplet builds a static libprotobuf, breaking the final link.
vcpkg_from_git(OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/ValveSoftware/GameNetworkingSockets.git
    REF d534c19aa760df3fb75fd20db13ba1932b8a5463)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS FEATURES ice ENABLE_ICE)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(BUILD_STATIC_LIB ON)
    set(BUILD_SHARED_LIB OFF)
    set(PROTOBUF_STATIC ON)
else()
    set(BUILD_STATIC_LIB OFF)
    set(BUILD_SHARED_LIB ON)
    set(PROTOBUF_STATIC OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUSE_CRYPTO=OpenSSL
        -DBUILD_STATIC_LIB=${BUILD_STATIC_LIB}
        -DBUILD_SHARED_LIB=${BUILD_SHARED_LIB}
        -DProtobuf_USE_STATIC_LIBS=${PROTOBUF_STATIC}
        -DBUILD_TESTS=OFF
        -DBUILD_EXAMPLES=OFF
        -DBUILD_TOOLS=OFF
        ${FEATURE_OPTIONS}
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/GameNetworkingSockets")
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share" "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
