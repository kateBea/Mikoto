cmake_minimum_required(VERSION 3.22)

project(HelloDirect3D11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCE_FILES ./*.cc)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Link the engine
target_link_libraries(${PROJECT_NAME} PRIVATE Mikoto)

# Editor-specific includes
target_include_directories(${PROJECT_NAME} PUBLIC .)

# ======================================
# Post build (Copy Engine Core Resources)
# ======================================

# Path to your resources folder (relative to project root)
set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/Mikoto/Resources")

# Path where resources should be copied in the build folder
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RESOURCE_DIR}" "${OUTPUT_DIR}"
        COMMENT "Copying resources to build directory..."
)

