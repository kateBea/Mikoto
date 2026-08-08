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

#ifndef MIKOTO_RHI_SHADER_HH
#define MIKOTO_RHI_SHADER_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/ResourcePool.hh>

#include <Filesystem/File.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    struct ShaderModuleCreateDescription {
        // If text has to be UTF-8,
        // HLSL text specifically has to be wchar
        void* mShaderContents{};
        core::size_t mShaderContentsSize{};

        ShaderType mType{ ShaderType::eInvalid };
        ShaderLanguage mLanguage{ ShaderLanguage::eSlang };

        eastl::string mEntryPoint{ "main" };

        // For slang
        eastl::string mModulePath{ "" };
        eastl::string mModuleName{ "" };

        auto SetModulePath( eastl::string_view path ) -> ShaderModuleCreateDescription&;
        auto SetModuleName( eastl::string_view name ) -> ShaderModuleCreateDescription&;

        auto SetStage( ShaderType stage ) -> ShaderModuleCreateDescription&;
        auto SetLanguage( ShaderLanguage language ) -> ShaderModuleCreateDescription&;
        auto SetEntryPoint( eastl::string_view name ) -> ShaderModuleCreateDescription&;
        auto SetContents( filesystem::FileHandle file ) -> ShaderModuleCreateDescription&;
        auto SetContents( void* pContents, core::size_t contentsByteSize ) -> ShaderModuleCreateDescription&;
    };

    class IShaderModule : public DeviceObject {
    public:
        MKT_NODISCARD auto GetType() const -> ShaderType;

        MKT_NODISCARD virtual auto GetContents() const -> const void*;
        MKT_NODISCARD virtual auto GetContentsByteSize() const -> size_t;

        virtual auto DumpShaderCode() -> void;

        ~IShaderModule() override = default;

    protected:
        explicit IShaderModule( ShaderType stage, eastl::string_view entryPoint, ShaderLanguage language );

    protected:
        eastl::string mEntryPoint{};
        ShaderType mStage{ ShaderType::eInvalid };
        ShaderLanguage mLanguage{ ShaderLanguage::eInvalid };
    };

    using ShaderModuleHandle = core::Ref<IShaderModule>;
}

#endif//MIKOTO_RHI_SHADER_HH
