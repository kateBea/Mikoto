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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_SHADER_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_SHADER_HH

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

namespace mikoto::renderer::d3d12 {

    class Shader final : public rhi::IShaderModule {
    public:
        explicit Shader(const rhi::ShaderModuleCreateDescription& desc);

        MKT_NODISCARD auto GetType() const -> rhi::ShaderType override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType object ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType object ) const -> rhi::Object override;

        MKT_NODISCARD auto GetContents() const -> const void* override;
        MKT_NODISCARD auto GetContentsByteSize() const -> core::usize override;

        MKT_NODISCARD auto GetSourceName() const -> eastl::string_view;

        auto DumpShaderCode() -> void override;

        ~Shader() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

        // [Internal usage]
        /** @brief Emits HLSL with Slang, then compiles it through PrepareForHlsl(). */
        auto PrepareForSlang() -> void;
        /** @brief Accepts native DXIL without entering either compiler. */
        auto PrepareForDxIL() -> void;
        /** @brief Compiles HLSL with the backend's shared DXC settings. */
        auto PrepareForHlsl() -> void;

    private:
        filesystem::Path mPath{};
        eastl::string mSource{};

        eastl::string mEntryPoint{};
        rhi::ShaderType mStage{ rhi::ShaderType::eInvalid };
        rhi::ShaderLanguage mLanguage{ rhi::ShaderLanguage::eInvalid };

        Microsoft::WRL::ComPtr<IDxcBlob> mBytecode{};

        eastl::string mModulePath{ "" };
        eastl::string mModuleName{ "" };

#if !defined(NDEBUG)
        eastl::string mShaderCode{};
#endif
    };

}

#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_SHADER_HH

