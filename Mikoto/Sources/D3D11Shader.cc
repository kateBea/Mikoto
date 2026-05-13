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

#include <cstring>

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D11/D3D11Context.hh>
#include <Renderer/D3D11/D3D11Shader.hh>
#include <Renderer/D3D11/Direct3D11Helpers.hh>
#include <Renderer/D3D11/Direct3D11Libraries.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <d3dcompiler.h>

namespace mikoto::renderer::d3d11 {

    Shader::Shader( const rhi::ShaderModuleCreateDescription& desc )
        : IShaderModule{ desc.mType, desc.mEntryPoint }, mFile{ desc.mFile }, mUseSlang{ desc.mIsSlangShader } {
        if (mStage == ShaderType::eVertex) {
            mTarget = "vs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11VertexShader>{};
        }
        else if (mStage == ShaderType::ePixel) {
            mTarget = "ps_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11PixelShader>{};
        }
        else if (mStage == ShaderType::eCompute) {
            mTarget = "cs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11ComputeShader>{};
        }
        else if (mStage == ShaderType::eGeometry) {
            mTarget = "gs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11GeometryShader>{};
        }
        else if (mStage == ShaderType::eDomain) {
            mTarget = "hs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11HullShader>{};
        }
        else if (mStage == ShaderType::eHull) {
            mTarget = "ds_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11DomainShader>{};
        }
        else {
            MKT_ASSERT(false, "Unsupported shader stage");
        }
    }

    auto Shader::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::D3D11_D3DBlob:
                return Object(mBytecode.Get());

            case ObjectType::D3D11_Shader:
                return eastl::visit([](auto& shader) -> Object {
                    return Object(shader.Get());
                }, mShader);

            default:
                break;
        }

        return Object(nullptr);
    }

    auto Shader::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::D3D11_D3DBlob: return Object( mBytecode.Get() );
            default:;
        }

        return Object(nullptr);
    }

    auto Shader::GetProgram() -> slang::IComponentType* {
        return mProgram.get();
    }

    auto Shader::GetProgram() const -> slang::IComponentType* {
        return mProgram.get();
    }

    auto Shader::GetByteCode() -> ID3DBlob* {
        return mBytecode.Get();
    }

    auto Shader::GetByteCode() const -> ID3DBlob* {
        return mBytecode.Get();
    }

    auto Shader::DumpShaderCode() -> void {
#if !defined(NDEBUG)
        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_AQUA, "{}", mShaderCode );
#endif
    }

    auto Shader::Initialize() -> void {
        // Here we assume we will load the shader from a file

        // Create Slang module
        auto session{ RenderSystem::Get().GetSlangCurrentSession() };
        const eastl::string_view modulePath{ mFile->GetPath() };
        const eastl::string_view moduleName{ mFile->GetName() };

        mSlangModule = session->loadModuleFromSource( moduleName.data(), modulePath.data(), nullptr, nullptr );
        MKT_ASSERT( mSlangModule, string::Format( "Failed to load Slang module {}", modulePath.data()) );

        // Create program
        Slang::ComPtr<slang::IEntryPoint> entryPoint{};
        SlangResult res{ mSlangModule->findEntryPointByName( mEntryPoint.c_str(), entryPoint.writeRef() ) };
        MKT_ASSERT( SLANG_SUCCEEDED( res ) && entryPoint, "Entry point not found" );

        slang::IComponentType* components[] = {
            mSlangModule,
            entryPoint
        };
        SlangResult compositeResult{ session->createCompositeComponentType(
                components,
                2,
                mProgram.writeRef() ) };
        MKT_ASSERT( SLANG_SUCCEEDED( compositeResult ), "Failed to create composite component type" );

        // Get compiled bytecode
        Slang::ComPtr<slang::IBlob> code{};

        SlangResult getEntryPointCodeResult{ mProgram->getEntryPointCode(
                0,// entry index
                0,// target index
                code.writeRef() ) };
        MKT_ASSERT( SLANG_SUCCEEDED(getEntryPointCodeResult) && code, "Failed to get entry point code");

        // Store bytecode
        auto src{ as<const char*>( code->getBufferPointer() ) };
        auto sizeBytes{ code->getBufferSize() };

        Microsoft::WRL::ComPtr<ID3DBlob> errors{ nullptr };
        HRESULT hrCompile{ D3DCompile(
            src,
            sizeBytes,
            moduleName.data(),
            nullptr,
            nullptr,
            mEntryPoint.c_str(),
            mTarget.c_str(),
            D3DCOMPILE_ENABLE_STRICTNESS,
            MKT_D3D11_NO_FLAGS,
            &mBytecode,
            &errors
        ) };

        if (errors != nullptr) {
            MKT_CORE_LOGGER_WARN( "shader compile failed with message: {}", as<const char*>(errors->GetBufferPointer()) );
        }
        MKT_ASSERT( SUCCEEDED(hrCompile), "Failed to compile HLSL");

#if !defined(NDEBUG)
        mShaderCode = as<const char*>( code->getBufferPointer() );
#endif
        auto device3{ checked_cast<Device*>( mDevice )->GetDevice3() };

        HRESULT shaderCreateResult{};
        if ( mStage == ShaderType::eVertex ) {
            auto& shader{ eastl::get<Microsoft::WRL::ComPtr<ID3D11VertexShader>>( mShader ) };
            shaderCreateResult = device3->CreateVertexShader(
                    mBytecode->GetBufferPointer(),
                    mBytecode->GetBufferSize(),
                    nullptr,
                    &shader );
        }
        else if ( mStage == ShaderType::ePixel ) {
            auto& shader{ eastl::get<Microsoft::WRL::ComPtr<ID3D11PixelShader>>( mShader ) };
            shaderCreateResult = device3->CreatePixelShader(
                    mBytecode->GetBufferPointer(),
                    mBytecode->GetBufferSize(),
                    nullptr,
                    &shader );
        }
        else if ( mStage == ShaderType::eCompute ) {
            auto& shader{ eastl::get<Microsoft::WRL::ComPtr<ID3D11ComputeShader>>( mShader ) };
            shaderCreateResult = device3->CreateComputeShader(
                mBytecode->GetBufferPointer(),
                mBytecode->GetBufferSize(),
                    nullptr,
                    &shader );
        }

        MKT_D3D11_DEVICE_CHECK( shaderCreateResult , "Failed to create shader" );

        mIsAllocated = true;
    }

    auto Shader::Release() -> void {
        mIsAllocated = false;
    }

    Shader::~Shader() {
        if ( mIsAllocated ) {
            Release();
        }
    }
}// namespace mikoto::renderer::d3d11

#endif