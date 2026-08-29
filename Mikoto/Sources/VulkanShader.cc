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

#include <filesystem>
#include <fstream>

#include <volk.h>

#include <spirv_glsl.hpp>

#include <spirv_reflect.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Platform.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanShader.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD auto GetGlslFromSpirv(const u32* ptr, usize count) -> eastl::string {
        // Create the compiler instance with the SPIR-V data
        spirv_cross::CompilerGLSL glslCompiler(ptr, count);

        // Set options for the GLSL output
        spirv_cross::CompilerGLSL::Options options;
        options.version = 450;
        options.es = false;
        options.vulkan_semantics = true;

        glslCompiler.set_common_options(options);

        try {
            return glslCompiler.compile().c_str();
        }
        catch (const spirv_cross::CompilerError& e) {
            MKT_CORE_LOGGER_ERROR( "Error decompile vulkan shader. e.what(): {}", e.what() );
        }

        return {};
    }

    Shader::Shader( const ShaderModuleCreateDescription& desc )
        : IShaderModule{ desc.mType, desc.mEntryPoint, desc.mLanguage }, mModulePath{ desc.mModulePath }, mModuleName{ desc.mModuleName }
    {
        // Contents are expected to be UTF-8 string, HLSL specifically expected WCHAR cause windows things
        // I use an string because I do not know for certain client will keep contents alive until the creation of the shader
        // in the call to Initialize()
        mSlangContents = eastl::string{ (cstr)desc.mShaderContents, desc.mShaderContentsSize };
    }

    auto Shader::DumpShaderCode() -> void {
#if !defined(NDEBUG)
        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_AQUA, "{}", mShaderCode );
#endif
    }

    auto Shader::GetNativeHandle( ObjectType object ) -> Object {
        if (object != ObjectType::Vk_Shader) {
            return Object{ nullptr };
        }

        return Object( mModule );
    }

    auto Shader::GetNativeHandle( ObjectType object ) const -> Object {
        if (object != ObjectType::Vk_Shader) {
            return Object{ nullptr };
        }

        return Object( mModule );
    }

    auto Shader::GetPipelineInfo() const -> const VkPipelineShaderStageCreateInfo& {
        return mStageCreateInfo;
    }

    auto Shader::GetContents() const -> const void* {
        return mSlangSpirv->getBufferPointer();
    }

    auto Shader::GetContentsByteSize() const -> size_t {
        return mSlangSpirv->getBufferSize();
    }

    auto Shader::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        switch (mLanguage) {
            case ShaderLanguage::eSlang:
                CompileForSlang();
                break;
            default:
                MKT_ASSERT( false, "Only slang supported for vulkan now." );
        }

        mIsAllocated = true;
    }

    auto Shader::CompileForSlang() -> void {
        // https://docs.shader-slang.org/en/latest/compilation-api.html
        auto session{ RenderSystem::Get()->GetSlangCurrentSession() };
        const eastl::string_view modulePath{ mModulePath };
        const eastl::string_view moduleName{ mModuleName };

        Slang::ComPtr<slang::IBlob> diagnosticsBlob{};

        // Attempt to load from provided string
        mSlangModule = session->loadModuleFromSourceString( moduleName.data(), modulePath.data(), mSlangContents.c_str(), diagnosticsBlob.writeRef() );

        // If fails we attempt to load from path
        if ( !mSlangModule ) {
            // Show fail reasons if any
            if ( diagnosticsBlob ) {
                cstr errorMessages = ( cstr )diagnosticsBlob->getBufferPointer();
                MKT_CORE_LOGGER_ERROR( "Vulkan shader Error loading slang from contents: \n{}", errorMessages );
            }

            mSlangModule = session->loadModuleFromSource( moduleName.data(), modulePath.data(), nullptr, diagnosticsBlob.writeRef() );
            if ( !mSlangModule ) {
                if ( diagnosticsBlob ) {
                    cstr errorMessages = ( cstr )diagnosticsBlob->getBufferPointer();
                    MKT_CORE_LOGGER_ERROR( "Vulkan shader Error loading slang from source: \n{}", errorMessages );
                }
                return;
            }
        }

        mSlangModule->getTargetCode( 0, mSlangSpirv.writeRef() );

        auto src{ mSlangSpirv->getBufferPointer() };
        auto size{ mSlangSpirv->getBufferSize() };

#if MIKOTO_DEBUG
        // Check client specified correctly the stage
        SpvReflectShaderModule module{};
        SpvReflectResult result{ spvReflectCreateShaderModule(size, src, &module) };
        if (result == SPV_REFLECT_RESULT_SUCCESS) {
            VkShaderStageFlagBits moduleStage{ as<VkShaderStageFlagBits>( module.shader_stage ) };
            if (GetShaderModuleStage( mStage ) != moduleStage) {
                mStage = GetShaderModuleStage( moduleStage );
                MKT_CORE_LOGGER_WARN( "Specified wrong stage for shader {}. Changed to right type.", mModulePath.c_str() );
            }
        }
#endif

        VkShaderModuleCreateInfo moduleCreateInfo{ initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = size;
        moduleCreateInfo.pCode = as<u32*>( src );

        MKT_VK_CHECK( vkCreateShaderModule(
            checked_cast<Device*>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( moduleCreateInfo ),
            nullptr,
            MKT_ADDRESSOF( mModule ) ));

        mStageCreateInfo = initializers::PipelineShaderStageCreateInfo();
        mStageCreateInfo.stage = GetShaderModuleStage( mStage );
        mStageCreateInfo.module = mModule;
        mStageCreateInfo.pName = mEntryPoint.c_str();
        mStageCreateInfo.flags = 0;
        mStageCreateInfo.pNext = nullptr;
        mStageCreateInfo.pSpecializationInfo = nullptr;

#if !defined(NDEBUG)
        // For debug purposes I store as GLSL
        mShaderCode = GetGlslFromSpirv( as<const u32*>(mSlangSpirv->getBufferPointer()),
            as<usize>(mSlangSpirv->getBufferSize()) / MKT_SIZEOF( u32 ) );
#endif

        // Cleanup
        mSlangContents.clear();
        mSlangContents.shrink_to_fit();

        mModulePath.clear();
        mModulePath.shrink_to_fit();

        mModuleName.clear();
        mModuleName.shrink_to_fit();
    }

    auto Shader::Release() -> void {
        vkDestroyShaderModule( checked_cast<Device*>( mDevice )->GetDevice(), mModule, nullptr );
        mIsAllocated = false;
    }

    Shader::~Shader() {
        if ( mIsAllocated ) {
            Release();
        }
    }
}