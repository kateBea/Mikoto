# Open Image Denoise networks executed by Vulkan compute. Build from source so
# vk_oidn inherits Mikoto's selected MSVC runtime instead of the SDK's /MD ABI.
if(MIKOTO_ENABLE_OIDN)
    set(VK_OIDN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(VK_OIDN_INSTALL OFF CACHE BOOL "" FORCE)
    set(VK_OIDN_EMBED_WEIGHTS ON CACHE BOOL "" FORCE)
    FetchContent_Declare(mikoto_oidn
        URL https://codeload.github.com/nvpro-samples/vk_oidn/zip/refs/tags/v0.2.0
        URL_HASH SHA256=f223e140cbda21d4bd7de8913e19148433dc0c90cc1b6cf10b8e663c10c175ac
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mikoto_oidn)
    target_link_libraries(Mikoto PRIVATE vk_oidn::vk_oidn)
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_OIDN=1)
else()
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_OIDN=0)
endif()
