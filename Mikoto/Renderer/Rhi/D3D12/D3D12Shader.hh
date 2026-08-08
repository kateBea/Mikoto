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

#ifndef MIKOTO_D3D12_SHADER_HH
#define MIKOTO_D3D12_SHADER_HH

#include <EASTL/string.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>
#include <dxcapi.h>

namespace mikoto::renderer::d3d12 {

    class Shader final : public rhi::IShaderModule {
    public:
        explicit Shader(const rhi::ShaderModuleCreateDescription& desc);

        auto DumpShaderCode() -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType object ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType object ) const -> rhi::Object override;

        MKT_NODISCARD auto GetContents() const -> const void* override;
        MKT_NODISCARD auto GetContentsByteSize() const -> size_t override;

        ~Shader() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        filesystem::Path mPath{};

        Slang::ComPtr<slang::IModule> mModule{};
        Slang::ComPtr<slang::IComponentType> mProgram {};

        Microsoft::WRL::ComPtr<IDxcBlob> mBytecode{};

        memory::BufferSpanHandle mContents{};

        eastl::string mModulePath{ "" };
        eastl::string mModuleName{ "" };

#if !defined(NDEBUG)
        eastl::string mShaderCode{};
#endif
    };

}

#endif

#endif//MIKOTO_D3D12_SHADER_HH
