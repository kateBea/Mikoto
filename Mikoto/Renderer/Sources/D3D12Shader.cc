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

#include <EASTL/array.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

// D3D12 extension library.
#include <Platform/Windows/Windows.hh>

#include <d3dcompiler.h>
#include <directx/d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/D3D12Shader.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Libraries.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    Shader::Shader( const rhi::ShaderModuleCreateDescription& desc )
        : mEntryPoint{ desc.mEntryPoint }, mStage{ desc.mType }, mLanguage{ desc.mLanguage }, mModulePath{ desc.mModulePath }, mModuleName{ desc.mModuleName } {
        MKT_BEGIN_PROFILER();
        if ( mLanguage == ShaderLanguage::eDXIL ) {
            Microsoft::WRL::ComPtr<IDxcUtils> utils{};
            ThrowIfFailed( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &utils ) ) );
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob{};
            ThrowIfFailed( utils->CreateBlob( desc.mShaderContents,
                static_cast<UINT32>( desc.mShaderContentsSize ), DXC_CP_ACP, &blob ) );
            mBytecode = blob;
        } else if ( desc.mShaderContents && desc.mShaderContentsSize ) {
            mSource.assign( static_cast<const char*>( desc.mShaderContents ), desc.mShaderContentsSize );
        }
    }

    auto Shader::DumpShaderCode() -> void {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_AQUA, "{}", mShaderCode );
#endif
    }

    auto Shader::GetNativeHandle( rhi::ObjectType object ) -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IShaderModule::GetNativeHandle( object );
    }

    auto Shader::GetNativeHandle( rhi::ObjectType object ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IShaderModule::GetNativeHandle( object );
    }

    auto Shader::GetContents() const -> const void* {
        MKT_BEGIN_PROFILER();
        return mBytecode->GetBufferPointer();
    }

    auto Shader::GetContentsByteSize() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mBytecode->GetBufferSize();
    }

    Shader::~Shader() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Shader::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    auto Shader::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        switch ( mLanguage ) {
            case renderer::rhi::ShaderLanguage::eSlang:
                PrepareForSlang();
                break;
            case renderer::rhi::ShaderLanguage::eHLSL:
                PrepareForHlsl();
                break;
            case renderer::rhi::ShaderLanguage::eDXIL:
                PrepareForDxIL();
                break;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Unsupported shader language for D3D12 Shader." );
        }

        if ( !mIsAllocated ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "D3D12 shader '{}' failed to initialize (entry point '{}').", mModulePath, mEntryPoint ).c_str() );
        }
    }

    auto Shader::PrepareForSlang() -> void {
        MKT_BEGIN_PROFILER();

        // Slang produces HLSL; release its session before invoking DXC.
        // https://docs.shader-slang.org/en/latest/compilation-api.html
        {
            auto lease{ RenderSystem::Get().AcquireSlangSession() };
            auto* session{ lease.GetSession( mStage ) };
            Slang::ComPtr<slang::IModule> module{};
            Slang::ComPtr<slang::IComponentType> program{};
            Slang::ComPtr<slang::IBlob> diagnostics{};

            module = session->loadModuleFromSource( mModuleName.c_str(), mModulePath.c_str(), nullptr, diagnostics.writeRef() );

            if ( !module ) {
                MKT_THROW_RUNTIME_ERROR( diagnostics ? static_cast<const char*>( diagnostics->getBufferPointer() ) : "Cannot load D3D12 shader module." );
            }

            // Compose only the requested entry point and stage.
            Slang::ComPtr<slang::IEntryPoint> entryPoint{};
            SlangResult res{ module->findEntryPointByName( mEntryPoint.c_str(), entryPoint.writeRef() ) };

            if ( !SLANG_SUCCEEDED( res ) || !entryPoint ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot find D3D12 shader entry point." );
            }

            const eastl::array<slang::IComponentType*, 2> components{ module.get(), entryPoint.get() };
            SlangResult compositeResult{ session->createCompositeComponentType(
                components.data(),
                core::as<SlangInt>( components.size() ),
                program.writeRef(), diagnostics.writeRef() ) };

            if ( !SLANG_SUCCEEDED( compositeResult ) || !program ) {
                MKT_THROW_RUNTIME_ERROR( diagnostics ? static_cast<const char*>( diagnostics->getBufferPointer() ) : "Cannot compose D3D12 shader program." );
            }

            // The D3D12 Slang target emits HLSL for the backend's DXC policy.
            Slang::ComPtr<slang::IBlob> code{};
            SlangResult getEntryPointCodeResult{ program->getEntryPointCode(
                0,// entry index
                0,// target index
                code.writeRef(), diagnostics.writeRef() ) };

            if ( !SLANG_SUCCEEDED( getEntryPointCodeResult ) || !code ) {
                MKT_THROW_RUNTIME_ERROR( diagnostics ? static_cast<const char*>( diagnostics->getBufferPointer() ) : "Cannot emit D3D12 shader HLSL." );
            }

            mSource.assign( static_cast<const char*>( code->getBufferPointer() ), code->getBufferSize() );
        }

        PrepareForHlsl();
    }

    auto Shader::PrepareForDxIL() -> void {
        MKT_BEGIN_PROFILER();

        // Cached DXIL is ready for pipeline creation and never enters Slang/DXC.
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_shader_bytecode
        mIsAllocated = mBytecode && mBytecode->GetBufferSize() != 0;
    }

    auto Shader::PrepareForHlsl() -> void {
        MKT_BEGIN_PROFILER();

        Context* ctx{ core::checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };
        auto description{ ShaderCompileDescription{}
            .SetSource( mSource )
            .SetSourceName( mModuleName )
            .SetEntryPoint( mEntryPoint )
            .SetProfile( mStage )
#if !defined( NDEBUG )
            .EnableDebug()
#else
            .StripReflection()
#endif
            .WarningsAsErrors() };
        mBytecode = ctx->GetShaderCompiler()->Compile( description );

#if !defined( NDEBUG )
        mShaderCode = mSource;
#endif

        mIsAllocated = true;
    }

    auto Shader::GetType() const -> rhi::ShaderType {
        return mStage;
    }

    auto Shader::GetSourceName() const -> eastl::string_view {
        return mModulePath;
    }
}
#endif
