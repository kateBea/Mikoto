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

#ifndef MIKOTO_SHADER_LIBRARY_HH
#define MIKOTO_SHADER_LIBRARY_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>

namespace mikoto::material {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::filesystem;

    inline constexpr eastl::string_view kSlangFileExtension{ ".slang" };

    struct ShaderLibraryDescription {
        renderer::rhi::IGpuDevice* mDevice{};
        Path mRootPath{};
    };

    // TODO: Move to renderer namespace
    // Caches shaders loaded from disk files for a given GPU device
    // It is a singleton for now as Mikoto only manages one GPU device across the entire application
    class ShaderLibrary final : public IService, public Singleton<ShaderLibrary> {
    public:
        explicit ShaderLibrary( const ShaderLibraryDescription &options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // This uri must be relative to the root path otherwise must be a full path
        auto GetShader( eastl::string_view uri ) -> renderer::rhi::ShaderModuleHandle;

        // TODO: extend to cached the compiled shaders
        auto LoadShader( eastl::string_view uri, rhi::ShaderType type ) -> renderer::rhi::ShaderModuleHandle;

        ~ShaderLibrary() override = default;

    private:
        Path mRootPath{};
        renderer::rhi::IGpuDevice *mDevice{ nullptr };
        ankerl::unordered_dense::map<Path, rhi::ShaderModuleHandle> mShaders{};
    };

}// namespace Mikoto

#endif // MIKOTO_SHADER_LIBRARY_HH
