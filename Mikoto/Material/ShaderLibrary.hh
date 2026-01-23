//    Copyright 2025 ケイト
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

#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Material/ShaderModule.hh>

namespace Mikoto {

    struct ShaderLibraryDescription {
        Path RootPath{};
        GpuDevice* Device{ nullptr };
    };

    // The shader library keeps track of the shaders loaded from the disk
    class ShaderLibrary final : public IService, public Singleton<ShaderLibrary> {
    public:
        explicit ShaderLibrary( const ShaderLibraryDescription &options );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto GetShader( std::string_view uri ) -> ShaderModuleHandle;
        auto LoadShader( const ShaderModuleDescription &loadInfo ) -> ShaderModuleHandle;
        auto LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle;

        ~ShaderLibrary() override = default;

    private:
        Path m_RootPath{};
        GpuDevice *m_Device{ nullptr };
        ankerl::unordered_dense::map<std::string, ShaderModuleHandle> m_Shaders{};
    };

}// namespace Mikoto

#endif // MIKOTO_SHADER_LIBRARY_HH
