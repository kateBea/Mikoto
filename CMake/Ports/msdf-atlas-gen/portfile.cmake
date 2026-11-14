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

vcpkg_from_git(OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/Chlumsky/msdf-atlas-gen.git
    REF 2ede254314a2512252a225fa6c975948d6af559a)
# Preserve Artery export support from the upstream submodule.
vcpkg_from_git(OUT_SOURCE_PATH ARTERY_SOURCE
    URL https://github.com/Chlumsky/artery-font-format.git
    REF 888674220216d1d326c6f29cf89165b545279c1f)
file(COPY "${ARTERY_SOURCE}/" DESTINATION "${SOURCE_PATH}/artery-font-format")
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "dynamic" DYNAMIC_RUNTIME)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS
    -DMSDF_ATLAS_BUILD_STANDALONE=OFF -DMSDF_ATLAS_USE_VCPKG=OFF
    -DMSDF_ATLAS_USE_SKIA=OFF -DMSDF_ATLAS_MSDFGEN_EXTERNAL=ON
    -DMSDF_ATLAS_INSTALL=ON -DMSDF_ATLAS_DYNAMIC_RUNTIME=${DYNAMIC_RUNTIME})
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/msdf-atlas-gen)
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/msdf-atlas-gen/msdf-atlas-gen-config.cmake"
    "find_dependency(msdfgen REQUIRED)" "find_dependency(msdfgen REQUIRED)\nfind_dependency(Threads REQUIRED)")
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
