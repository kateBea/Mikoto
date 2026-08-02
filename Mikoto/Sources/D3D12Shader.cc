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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <dxcapi.h>

#include <Renderer/D3D12/D3D12Shader.hh>
#include <Renderer/D3D12/D3D12Context.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>
#include <Renderer/D3D12/Direct3D12Libraries.hh>

namespace mikoto::renderer::d3d12 {

    Shader::Shader( const rhi::ShaderModuleCreateDescription &desc )
        : IShaderModule{ desc.mType, desc.mEntryPoint }, mFile{ desc.mFile }, mUseSlang{ desc.mIsSlangShader }
    {

    }

    auto Shader::DumpShaderCode() -> void {
#if !defined(NDEBUG)
        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_AQUA, "{}", mShaderCode );
#endif
    }

    auto Shader::GetNativeHandle( rhi::ObjectType object ) -> rhi::Object {
        return IShaderModule::GetNativeHandle( object );
    }

    auto Shader::GetNativeHandle( rhi::ObjectType object ) const -> rhi::Object {
        return IShaderModule::GetNativeHandle( object );
    }

    auto Shader::GetContents() const -> const void * {
        return mBytecode->GetBufferPointer();
    }

    auto Shader::GetContentsByteSize() const -> size_t {
        return mBytecode->GetBufferSize();
    }

    Shader::~Shader() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Shader::Release() -> void {
        mIsAllocated = false;
    }

    auto Shader::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // https://docs.shader-slang.org/en/latest/compilation-api.html
        // Create Slang module
        auto session{ RenderSystem::Get().GetSlangCurrentSession() };
        const eastl::string_view modulePath{ mFile->GetPath() };
        const eastl::string_view moduleName{ mFile->GetName() };

        mModule = session->loadModuleFromSource( moduleName.data(), modulePath.data(), nullptr, nullptr );
        MKT_ASSERT( mModule, string::Format( "Failed to load Slang module {}", modulePath.data()) );

        // Create program
        Slang::ComPtr<slang::IEntryPoint> entryPoint{};
        SlangResult res{ mModule->findEntryPointByName( mEntryPoint.c_str(), entryPoint.writeRef() ) };
        MKT_ASSERT( SLANG_SUCCEEDED( res ) && entryPoint, "Entry point not found" );

        slang::IComponentType* components[]{
            mModule,
            entryPoint };
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

        Context* ctx{ checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };
        auto description{ ShaderCompileDescription{}
            .SetSource(src)
            .SetSourceName(moduleName)
            .SetEntryPoint(mEntryPoint)
            .SetProfile(mStage)
#if !defined(NDEBUG)
            .EnableDebug()
            .SkipOptimizations()
#else
            .StripReflection()
#endif
            .WarningsAsErrors()
            .AddDefinition("TEST_MACRO1", "1")
            .AddDefinition("TEST_MACRO2", "256") };
        mBytecode = ctx->GetShaderCompiler()->Compile(description);

#if !defined(NDEBUG)
        mShaderCode = as<const char*>( code->getBufferPointer() );
#endif

        mIsAllocated = true;
    }
}// namespace mikoto::renderer::d3d12

#endif