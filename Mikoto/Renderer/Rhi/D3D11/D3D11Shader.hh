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

#ifndef MIKOTO_RENDERER_RHI_D3D11_D3D11_SHADER_HH
#define MIKOTO_RENDERER_RHI_D3D11_D3D11_SHADER_HH

#include <EASTL/variant.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <d3d11.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

namespace mikoto::renderer::d3d11 {

    class Shader final : public rhi::IShaderModule {
    public:
        auto GetType() const -> rhi::ShaderType override;
        auto GetContents() const -> const void* override;
        auto GetContentsByteSize() const -> core::usize override;

    private:
        eastl::string mEntryPoint{};
        rhi::ShaderType mStage{ rhi::ShaderType::eInvalid };
        rhi::ShaderLanguage mLanguage{ rhi::ShaderLanguage::eInvalid };

    public:
        explicit Shader( const rhi::ShaderModuleCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType ) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object override;


        MKT_NODISCARD auto GetByteCode() -> ID3DBlob*;
        MKT_NODISCARD auto GetByteCode() const -> ID3DBlob*;

        auto DumpShaderCode() -> void override;

        ~Shader() override;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;


    private:
        eastl::variant<
            Microsoft::WRL::ComPtr<ID3D11VertexShader>,
            Microsoft::WRL::ComPtr<ID3D11PixelShader>,
            Microsoft::WRL::ComPtr<ID3D11HullShader>,
            Microsoft::WRL::ComPtr<ID3D11DomainShader>,
            Microsoft::WRL::ComPtr<ID3D11GeometryShader>,
            Microsoft::WRL::ComPtr<ID3D11ComputeShader>> mShader{};


        eastl::string mTarget{};
        Microsoft::WRL::ComPtr<ID3DBlob> mBytecode{};

        eastl::string mModulePath{ "" };
        eastl::string mModuleName{ "" };

#if !defined(NDEBUG)
        eastl::string mShaderCode{};
#endif
    };
}// namespace mikoto::renderer::d3d11

#endif

#endif// MIKOTO_RENDERER_RHI_D3D11_D3D11_SHADER_HH

