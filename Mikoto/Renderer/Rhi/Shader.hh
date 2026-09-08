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

    /**
     * Creation parameters for a compiled or source shader module.
     */
    struct ShaderModuleCreateDescription {
        // If text has to be UTF-8,
        // HLSL text specifically has to be wchar
        void* mShaderContents{};
        core::usize mShaderContentsSize{};

        ShaderType mType{ ShaderType::eInvalid };
        ShaderLanguage mLanguage{ ShaderLanguage::eSlang };

        eastl::string mEntryPoint{ "main" };

        // For slang
        eastl::string mModulePath{ "" };
        eastl::string mModuleName{ "" };

        /**
         * Sets the value handled by SetModulePath.
         *
         * @param path Input value used by this operation.
         * @returns The result of SetModulePath.
         */
        auto SetModulePath( eastl::string_view path ) -> ShaderModuleCreateDescription&;

        /**
         * Sets the value handled by SetModuleName.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetModuleName.
         */
        auto SetModuleName( eastl::string_view name ) -> ShaderModuleCreateDescription&;

        /**
         * Sets the value handled by SetStage.
         *
         * @param stage Input value used by this operation.
         * @returns The result of SetStage.
         */
        auto SetStage( ShaderType stage ) -> ShaderModuleCreateDescription&;

        /**
         * Sets the value handled by SetLanguage.
         *
         * @param language Input value used by this operation.
         * @returns The result of SetLanguage.
         */
        auto SetLanguage( ShaderLanguage language ) -> ShaderModuleCreateDescription&;

        /**
         * Sets the value handled by SetEntryPoint.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetEntryPoint.
         */
        auto SetEntryPoint( eastl::string_view name ) -> ShaderModuleCreateDescription&;

        /**
         * Sets the value handled by SetContents.
         *
         * @param file Input value used by this operation.
         * @returns The result of SetContents.
         */
        auto SetContents( filesystem::FileHandle file ) -> ShaderModuleCreateDescription&;

        /**
         * Sets the value handled by SetContents.
         *
         * @param pContents Input value used by this operation.
         * @param contentsByteSize Input value used by this operation.
         * @returns The result of SetContents.
         */
        auto SetContents( void* pContents, core::usize contentsByteSize ) -> ShaderModuleCreateDescription&;
    };

    /**
     * Backend-independent shader module used by pipeline descriptions.
     */
    class IShaderModule : public DeviceObject {
    public:

        /**
         * Returns the shader stage represented by this module.
         * @returns The result of GetType.
         */
        MKT_NODISCARD auto GetType() const -> ShaderType;

        /**
         * Returns module bytes when the implementation retains them.
         * @returns The result of GetContents.
         */
        MKT_NODISCARD virtual auto GetContents() const -> const void*;

        /**
         * Returns the retained module byte count.
         * @returns The result of GetContentsByteSize.
         */
        MKT_NODISCARD virtual auto GetContentsByteSize() const -> core::usize;

        /**
         * Emits a human-readable representation of the shader, when supported.
         */
        virtual auto DumpShaderCode() -> void;

        /**
         * Destroys the shader module.
         */
        ~IShaderModule() override = default;

    protected:

        /**
         * Constructs a shader module.
         *
         * @param stage Shader stage implemented by the module.
         * @param entryPoint Entry-point function name.
         * @param language Source or bytecode language.
         */
        explicit IShaderModule( ShaderType stage, eastl::string_view entryPoint, ShaderLanguage language );

    protected:
        eastl::string mEntryPoint{};
        ShaderType mStage{ ShaderType::eInvalid };
        ShaderLanguage mLanguage{ ShaderLanguage::eInvalid };
    };

    using ShaderModuleHandle = core::Ref<IShaderModule>;
}

#endif//MIKOTO_RHI_SHADER_HH
