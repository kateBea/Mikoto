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

#ifndef MIKOTO_D3D11SHADER_HH
#define MIKOTO_D3D11SHADER_HH

#include <EASTL/variant.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>

#include <Renderer/D3D11/Direct3D11Helpers.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <d3d11.h>
#include <wrl.h>

namespace mikoto::renderer::d3d11 {

    class Shader final : public rhi::IShaderModule {
    public:
        explicit Shader( const rhi::ShaderModuleCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( ObjectType ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetProgram() -> slang::IComponentType*;
        MKT_NODISCARD auto GetProgram() const -> slang::IComponentType*;

        MKT_NODISCARD auto GetByteCode() -> ID3DBlob*;
        MKT_NODISCARD auto GetByteCode() const -> ID3DBlob*;

        auto DumpShaderCode() -> void override;

        ~Shader() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;


    private:
        eastl::variant<
            Microsoft::WRL::ComPtr<ID3D11VertexShader>,
            Microsoft::WRL::ComPtr<ID3D11PixelShader>,
            Microsoft::WRL::ComPtr<ID3D11HullShader>,
            Microsoft::WRL::ComPtr<ID3D11DomainShader>,
            Microsoft::WRL::ComPtr<ID3D11GeometryShader>,
            Microsoft::WRL::ComPtr<ID3D11ComputeShader>> mShader{};

        Slang::ComPtr<slang::IComponentType> mProgram {};
        Slang::ComPtr<slang::IModule> mSlangModule{};

        eastl::string mTarget{};
        Microsoft::WRL::ComPtr<ID3DBlob> mBytecode{};

        filesystem::FileHandle mFile{};

        bool mUseSlang{ true };

#if !defined(NDEBUG)
        eastl::string mShaderCode{};
#endif

    };
}// namespace mikoto::renderer::d3d11

#endif

#endif//MIKOTO_D3D11SHADER_HH
