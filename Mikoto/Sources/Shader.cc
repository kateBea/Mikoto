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

#include <Renderer/Rhi/Shader.hh>

namespace mikoto::renderer::rhi {

    auto IShaderModule::GetType() const -> ShaderType {
        return mStage;
    }

    auto IShaderModule::GetContents() const -> const void * {
        return nullptr;
    }

    auto IShaderModule::GetContentsByteSize() const -> size_t {
        return 0;
    }

    auto IShaderModule::DumpShaderCode() -> void {

    }

    IShaderModule::IShaderModule( ShaderType stage, eastl::string_view entryPoint, ShaderLanguage language )
        : mEntryPoint{ entryPoint }, mStage{ stage }, mLanguage{ language } {

    }

    auto ShaderModuleCreateDescription::SetModulePath( eastl::string_view path ) -> ShaderModuleCreateDescription & {
        mModulePath = path;
        return *this;
    }

    auto ShaderModuleCreateDescription::SetModuleName( eastl::string_view name ) -> ShaderModuleCreateDescription & {
        mModuleName = name;
        return *this;
    }

    auto ShaderModuleCreateDescription::SetLanguage( ShaderLanguage language ) -> ShaderModuleCreateDescription & {
        mLanguage = language;
        return *this;
    }

    auto ShaderModuleCreateDescription::SetContents( filesystem::FileHandle file ) -> ShaderModuleCreateDescription & {
        return SetContents(file->GetContentsBytes(), file->GetSize());
    }

    auto ShaderModuleCreateDescription::SetContents( void *pContents, core::usize contentsByteSize ) -> ShaderModuleCreateDescription & {
        mShaderContents = pContents;
        mShaderContentsSize = contentsByteSize;

        return *this;
    }

    auto ShaderModuleCreateDescription::SetStage( ShaderType stage ) -> ShaderModuleCreateDescription & {
        mType = stage;
        return *this;
    }

    auto ShaderModuleCreateDescription::SetEntryPoint( eastl::string_view name ) -> ShaderModuleCreateDescription & {
        mEntryPoint = name;
        return *this;
    }
}