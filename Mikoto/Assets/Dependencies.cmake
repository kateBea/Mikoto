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
    find_package(assimp CONFIG REQUIRED)
    find_package(meshoptimizer CONFIG REQUIRED)
    find_package(fastgltf 1.0.0 CONFIG REQUIRED)
    find_package(Ktx CONFIG REQUIRED)
    find_package(mikoto-compressonator CONFIG REQUIRED)
    find_package(Stb REQUIRED)

    target_link_libraries(Mikoto PUBLIC fastgltf::fastgltf assimp::assimp meshoptimizer::meshoptimizer KTX::ktx Compressonator::CMP_Core)
    target_include_directories(Mikoto PUBLIC "${Stb_INCLUDE_DIR}")
else()
    # Assimp
    set(ASSIMP_INSTALL OFF CACHE BOOL "")
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "")
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
    set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "")
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "")
    FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.5
    )
    FetchContent_MakeAvailable(assimp)

    # stb
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    )
    FetchContent_MakeAvailable(stb)

    # MeshOptimizer
    set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
    set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(MESHOPT_STABLE_EXPORTS OFF CACHE BOOL "" FORCE)
    set(MESHOPT_WERROR OFF CACHE BOOL "" FORCE)
    set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG        v1.3
    )

    FetchContent_MakeAvailable(meshoptimizer)

    # KTX-Software
    set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_PY OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_JS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TOOLS_CTS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_KTX2 ON CACHE BOOL "" FORCE)
    set(KTX_FEATURE_WRITE ON CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TRANSCODE ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.4.2
    )

    FetchContent_MakeAvailable(ktx)

    # Static CPU codecs only: no CLI, GUI, GPU plugins or shared runtime.
    set(PROJECT_FOLDER_SDK_LIBS "Dependencies/Compressonator")
    FetchContent_Declare(compressonator
        GIT_REPOSITORY https://github.com/GPUOpen-Tools/compressonator.git
        GIT_TAG V4.5.52
        SOURCE_SUBDIR cmp_core)
    FetchContent_MakeAvailable(compressonator)

    # Upstream spells the MSVC switch /arch:AVX-512; cl expects /arch:AVX512.
    if(MSVC)
        set_property(TARGET CMP_Core_AVX512 PROPERTY COMPILE_OPTIONS /arch:AVX512)
    endif()

    FetchContent_Declare(fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG v1.0.0)
    FetchContent_MakeAvailable(fastgltf)

    target_link_libraries(Mikoto PUBLIC assimp meshoptimizer fastgltf::fastgltf ktx CMP_Core)
    target_include_directories(Mikoto PUBLIC
        "${stb_SOURCE_DIR}" "${ktx_SOURCE_DIR}")
endif()
