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

#include <array>
#include <cstring>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Shader.hh>
#include <Renderer/Rhi/D3D11/D3D11Context.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Libraries.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <d3dcompiler.h>

namespace mikoto::renderer::d3d11 {

    Shader::Shader( const rhi::ShaderModuleCreateDescription& desc )
        : mEntryPoint{ desc.mEntryPoint }, mStage{ desc.mType }, mLanguage{ desc.mLanguage }, mModulePath{ desc.mModulePath }, mModuleName{ desc.mModuleName } {
        MKT_BEGIN_PROFILER();

        if ( mStage == renderer::rhi::ShaderType::eVertex ) {
            mTarget = "vs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11VertexShader>{};
        }
        else if ( mStage == renderer::rhi::ShaderType::ePixel ) {
            mTarget = "ps_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11PixelShader>{};
        }
        else if ( mStage == renderer::rhi::ShaderType::eCompute ) {
            mTarget = "cs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11ComputeShader>{};
        }
        else if ( mStage == renderer::rhi::ShaderType::eGeometry ) {
            mTarget = "gs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11GeometryShader>{};
        }
        else if ( mStage == renderer::rhi::ShaderType::eDomain ) {
            mTarget = "hs_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11HullShader>{};
        }
        else if ( mStage == renderer::rhi::ShaderType::eHull ) {
            mTarget = "ds_5_0";
            mShader = Microsoft::WRL::ComPtr<ID3D11DomainShader>{};
        }
        else {
            MKT_ASSERT( false, "Unsupported shader stage" );
        }
    }

    auto Shader::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_D3DBlob:
                return renderer::rhi::Object( mBytecode.Get() );

            case renderer::rhi::ObjectType::D3D11_Shader:
                return eastl::visit( []( auto& shader ) -> renderer::rhi::Object {
                    return renderer::rhi::Object( shader.Get() );
                }, mShader );

            default:
                break;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Shader::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_D3DBlob:
                return renderer::rhi::Object( mBytecode.Get() );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Shader::GetByteCode() -> ID3DBlob* {
        MKT_BEGIN_PROFILER();
        return mBytecode.Get();
    }

    auto Shader::GetByteCode() const -> ID3DBlob* {
        MKT_BEGIN_PROFILER();
        return mBytecode.Get();
    }

    auto Shader::DumpShaderCode() -> void {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_AQUA, "{}", mShaderCode );
#endif
    }

    auto Shader::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        const eastl::string_view moduleName{ mModuleName };
        eastl::string source{};
        {
            auto lease{ RenderSystem::Get().AcquireSlangSession() };
            auto* session{ lease.GetSession( mStage ) };
            Slang::ComPtr<slang::IBlob> diagnostics{};
            const auto requireResult{ [&]( bool success, core::cstr operation ) {
                if ( !success ) {
                    MKT_THROW_RUNTIME_ERROR( string::Format( "D3D11 shader '{}', {}: {}", mModulePath, operation,
                        diagnostics ? static_cast<const char*>( diagnostics->getBufferPointer() ) : "Slang returned no diagnostics." ) );
                }
            } };
            Slang::ComPtr<slang::IModule> module{ session->loadModuleFromSource(
                moduleName.data(), mModulePath.c_str(), nullptr, diagnostics.writeRef() ) };
            requireResult( module != nullptr, "loading module" );

            Slang::ComPtr<slang::IEntryPoint> entryPoint{};
            const auto entryResult{ module->findEntryPointByName( mEntryPoint.c_str(), entryPoint.writeRef() ) };
            requireResult( SLANG_SUCCEEDED( entryResult ) && entryPoint, "finding entry point" );

            const std::array<slang::IComponentType*, 2> components{ module.get(), entryPoint.get() };
            Slang::ComPtr<slang::IComponentType> program{};
            const auto compositeResult{ session->createCompositeComponentType(
                components.data(), core::as<SlangInt>( components.size() ), program.writeRef(), diagnostics.writeRef() ) };
            requireResult( SLANG_SUCCEEDED( compositeResult ) && program, "creating shader program" );

            Slang::ComPtr<slang::IBlob> code{};
            const auto codeResult{ program->getEntryPointCode( 0, 0, code.writeRef(), diagnostics.writeRef() ) };
            requireResult( SLANG_SUCCEEDED( codeResult ) && code, "emitting HLSL" );
            source.assign( static_cast<const char*>( code->getBufferPointer() ), code->getBufferSize() );
        }

        Microsoft::WRL::ComPtr<ID3DBlob> errors{ nullptr };
        HRESULT hrCompile{ D3DCompile(
            source.data(),
            source.size(),
            moduleName.data(),
            nullptr,
            nullptr,
            mEntryPoint.c_str(),
            mTarget.c_str(),
            D3DCOMPILE_ENABLE_STRICTNESS,
            MKT_D3D11_NO_FLAGS,
            &mBytecode,
            &errors ) };

        if ( errors != nullptr ) {
            MKT_CORE_LOGGER_WARN( "shader compile failed with message: {}", core::as<const char*>( errors->GetBufferPointer() ) );
        }

        if ( FAILED( hrCompile ) || !mBytecode ) {
            MKT_THROW_RUNTIME_ERROR( "D3D11 failed to compile HLSL." );
        }

#if !defined( NDEBUG )
        mShaderCode = source;
#endif
        auto device3{ core::checked_cast<Device*>( mDevice )->GetDevice3() };

        HRESULT shaderCreateResult{};

        if ( mStage == renderer::rhi::ShaderType::eVertex ) {
            auto& shader{ eastl::get<Microsoft::WRL::ComPtr<ID3D11VertexShader>>( mShader ) };
            shaderCreateResult = device3->CreateVertexShader(
                mBytecode->GetBufferPointer(),
                mBytecode->GetBufferSize(),
                nullptr,
                &shader );
        }
        else if ( mStage == renderer::rhi::ShaderType::ePixel ) {
            auto& shader{ eastl::get<Microsoft::WRL::ComPtr<ID3D11PixelShader>>( mShader ) };
            shaderCreateResult = device3->CreatePixelShader(
                mBytecode->GetBufferPointer(),
                mBytecode->GetBufferSize(),
                nullptr,
                &shader );
        }
        else if ( mStage == renderer::rhi::ShaderType::eCompute ) {
            auto& shader{ eastl::get<Microsoft::WRL::ComPtr<ID3D11ComputeShader>>( mShader ) };
            shaderCreateResult = device3->CreateComputeShader(
                mBytecode->GetBufferPointer(),
                mBytecode->GetBufferSize(),
                nullptr,
                &shader );
        }

        MKT_D3D11_DEVICE_CHECK( shaderCreateResult, "Failed to create shader" );

        mIsAllocated = true;
    }

    auto Shader::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    Shader::~Shader() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Shader::GetType() const -> rhi::ShaderType {
        return mStage;
    }

    auto Shader::GetContents() const -> const void* {
        return nullptr;
    }

    auto Shader::GetContentsByteSize() const -> core::usize {
        return 0;
    }
}
#endif
