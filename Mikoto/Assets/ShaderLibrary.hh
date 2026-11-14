//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_ASSETS_SHADER_LIBRARY_HH
#define MIKOTO_ASSETS_SHADER_LIBRARY_HH

#include <mutex>
#include <future>
#include <condition_variable>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>

namespace mikoto::asset {

    /** File extension used by Slang shader source assets. */
    inline constexpr eastl::string_view kSlangFileExtension{ ".slang" };

    /**
     * @brief Configuration used to construct a ShaderLibrary.
     * @details The device is borrowed; the caller owns it and must keep it alive
     * until the library has been shut down. The root path is used by LoadShader()
     * to resolve relative shader URIs.
     */
    struct ShaderLibraryDescription {
        /** GPU device used to create shader modules. */
        renderer::rhi::IGpuDevice* mDevice{};

        /** Base directory for relative shader source URIs. */
        filesystem::Path mRootPath{};

        /** Persist backend bytecode and dependency metadata between runs. */
        bool mCacheShaders{ true };
        filesystem::Path mCachePath{ "Assets/.cache/Shaders" };
    };

    /**
     * @brief Loads and caches shader modules created from Slang source files.
     * @details ShaderLibrary owns the cached ShaderModuleHandle values, while the
     * GPU device and source files remain owned by their respective systems. A
     * shader is compiled on its first LoadShader() call and reused thereafter.
     */
    class ShaderLibrary final : public core::IService {
    public:
        /**
         * @brief Creates a shader library from its device and root directory.
         * @param options Device and source-root configuration.
         * The constructor only stores the supplied device pointer and root path;
         * shader I/O and compilation occur in LoadShader().
         */
        explicit ShaderLibrary( const ShaderLibraryDescription &options );


        /**
         * @brief Marks the library ready for use.
         * @details Initialization does not eagerly scan or compile the root directory.
         */
        auto Initialize() -> void override;


        /**
         * @brief Releases all cached shader module handles and marks the service stopped.
         * @details Calling Shutdown() more than once is harmless.
         */
        auto Shutdown() -> void override;


        /**
         * @brief Looks up an already loaded shader module.
         * @param uri Source path relative to the configured root directory.
         * @param type Shader stage to look up.
         * @param entryPoint Entry point that identifies the compiled variant.
         * @return The cached shader module, or an empty handle when it is not cached.
         * @note This method does not read from disk or compile a shader. Use
         * LoadShader() when a cache miss should load and compile the source.
         */
        auto GetShader(
            eastl::string_view uri,
            renderer::rhi::ShaderType type,
            eastl::string_view entryPoint = "main" ) -> renderer::rhi::ShaderModuleHandle;


        /**
         * @brief Loads, compiles, and caches a Slang shader module.
         * @param uri Path relative to the configured root path. The resulting
         * resolved path groups the cached stage and entry-point variants.
         * @param type Shader stage to compile, such as vertex or fragment.
         * @param entryPoint Entry point to compile from the source file.
         * @return The cached or newly created shader module; an empty handle if
         * creation fails.
         * @details Source contents are read through FileService. Compilation is
         * performed on demand. Concurrent requests for the same path, stage and
         * entry point share one compilation; unrelated variants may run in parallel.
         * When enabled, disk hits create modules directly from backend bytecode.
         * @throws Filesystem and renderer exceptions when the source cannot be
         * read or the device rejects shader creation.
         */
        auto LoadShader(
            eastl::string_view uri,
            renderer::rhi::ShaderType type,
            eastl::string_view entryPoint = "main" ) -> renderer::rhi::ShaderModuleHandle;


        /** Destructor; cached modules are released when the library is destroyed. */
        ~ShaderLibrary() override;

    private:
        struct ShaderVariant {
            eastl::string mEntryPoint{};
            renderer::rhi::ShaderType mStage{};
            std::shared_future<renderer::rhi::ShaderModuleHandle> mResult{};
        };

    private:
        /** Base directory used to resolve shader source URIs. */
        filesystem::Path mRootPath{};

        bool mCacheShaders{};
        filesystem::Path mCachePath{};

        /** Borrowed GPU device used for shader-module creation. */
        renderer::rhi::IGpuDevice *mDevice{ nullptr };

        /** A future covers both an in-flight compilation and its cached result. */
        ankerl::unordered_dense::map<filesystem::Path, eastl::vector<ShaderVariant>> mShaders{};
        std::mutex mShaderMutex{};
        std::condition_variable mCompilationsFinished{};
        core::usize mPendingCompilations{};
        bool mStopping{ true };
    };

}// namespace Mikoto

#endif// MIKOTO_ASSETS_SHADER_LIBRARY_HH
