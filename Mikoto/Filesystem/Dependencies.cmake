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
    find_package(nfd CONFIG REQUIRED)

    find_path(MIKOTO_PFD_INCLUDE_DIR portable-file-dialogs.h REQUIRED)

    target_link_libraries(Mikoto PUBLIC nfd::nfd)
    target_include_directories(Mikoto PUBLIC "${MIKOTO_PFD_INCLUDE_DIR}" "${MIKOTO_SOURCE_DIR}/Vendor/Filewatch")
else()
    # nativefiledialog-extended
    FetchContent_Declare(nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.4.1
    )
    FetchContent_MakeAvailable(nfd)

    # PFD
    FetchContent_Declare(
        pfd
        GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
        GIT_TAG 0.1.0
    )
    FetchContent_MakeAvailable(pfd)

    # Filewatch
    # Using https://github.com/ThomasMonkman/filewatch/tree/feature/fix-filesystem (currently working branch)
    # It is single header
    set(FILEWATCH_ROOT "${MIKOTO_SOURCE_DIR}/Vendor/Filewatch")
    set(FILEWATCH_INCLUDE_DIR "${FILEWATCH_ROOT}")

    target_link_libraries(Mikoto PUBLIC nfd portable_file_dialogs)
    target_include_directories(Mikoto PUBLIC "${FILEWATCH_INCLUDE_DIR}")
endif()
