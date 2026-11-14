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

#include <cstring>
#include <fstream>
#include <filesystem>

#include <shaderc/shaderc.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

#include <spirv_glsl.hpp>

#if defined( MKT_USE_PACKAGED_SPIRV_REFLECT )
#include <spirv-reflect/spirv_reflect.h>
#else
#include <spirv_reflect.h>
#endif

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanShader.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto GetShaderCShaderKind( renderer::rhi::ShaderType stage, shaderc_shader_kind& kind ) -> bool {
        MKT_BEGIN_PROFILER();

        switch ( stage ) {
            case renderer::rhi::ShaderType::eVertex:
                kind = shaderc_vertex_shader;
                return true;
            case renderer::rhi::ShaderType::ePixel:
                kind = shaderc_fragment_shader;
                return true;
            case renderer::rhi::ShaderType::eCompute:
                kind = shaderc_compute_shader;
                return true;
            case renderer::rhi::ShaderType::eGeometry:
                kind = shaderc_geometry_shader;
                return true;
            case renderer::rhi::ShaderType::eHull:
                kind = shaderc_tess_control_shader;
                return true;
            case renderer::rhi::ShaderType::eDomain:
                kind = shaderc_tess_evaluation_shader;
                return true;
            case renderer::rhi::ShaderType::eRayGeneration:
                kind = shaderc_raygen_shader;
                return true;
            case renderer::rhi::ShaderType::eIntersection:
                kind = shaderc_intersection_shader;
                return true;
            case renderer::rhi::ShaderType::eAnyHit:
                kind = shaderc_anyhit_shader;
                return true;
            case renderer::rhi::ShaderType::eClosestHit:
                kind = shaderc_closesthit_shader;
                return true;
            case renderer::rhi::ShaderType::eMiss:
                kind = shaderc_miss_shader;
                return true;
            case renderer::rhi::ShaderType::eInvalid:
            case renderer::rhi::ShaderType::eCount:
                return false;
        }

        return false;
    }

    MKT_NODISCARD static auto GetGlslFromSpirv( const core::u32* ptr, core::usize wordCount, core::cstr shaderPath ) -> eastl::string {
        MKT_BEGIN_PROFILER();

        try {
            // SPIRV-Cross parses in its constructor and counts u32 words, not
            // bytes. Keep parsing inside the handler too: this optional debug
            // representation must not abort startup if decompilation fails.
            spirv_cross::CompilerGLSL glslCompiler( ptr, wordCount );

            spirv_cross::CompilerGLSL::Options options{};
            options.version = 450;
            options.es = false;
            options.vulkan_semantics = true;
            glslCompiler.set_common_options( options );

            return glslCompiler.compile().c_str();
        } catch ( const spirv_cross::CompilerError& e ) {
            MKT_CORE_LOGGER_ERROR( "Cannot decompile Vulkan shader '{}' ({} SPIR-V words): {}", shaderPath, wordCount, e.what() );
        }

        return {};
    }

    Shader::Shader( const renderer::rhi::ShaderModuleCreateDescription& desc )
        : mEntryPoint{ desc.mEntryPoint }, mStage{ desc.mType }, mLanguage{ desc.mLanguage }, mModulePath{ desc.mModulePath }, mModuleName{ desc.mModuleName } {
        MKT_BEGIN_PROFILER();
        // Contents are expected to be UTF-8 string, HLSL specifically expected WCHAR cause windows things
        // I use an string because I do not know for certain client will keep contents alive until the creation of the shader
        // in the call to Initialize()

        switch ( mLanguage ) {
            case renderer::rhi::ShaderLanguage::eSlang:
                mSlangContents = eastl::string{ as<core::cstr>( desc.mShaderContents ), desc.mShaderContentsSize };
                break;
            case renderer::rhi::ShaderLanguage::eGLSL:
                mGlslContents = eastl::string{ core::as<const char*>( desc.mShaderContents ), desc.mShaderContentsSize };
                break;
            case renderer::rhi::ShaderLanguage::eSPIRV: {
                mSpirvContents = eastl::vector<core::u32>{
                    core::as<const core::u32*>( desc.mShaderContents ),
                    core::as<const core::u32*>( desc.mShaderContents ) + desc.mShaderContentsSize / MKT_SIZEOF( core::u32 )
                };
                break;
            }

            default:;
        }
    }

    auto Shader::DumpShaderCode() -> void {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_AQUA, "{}", mShaderCode );
#endif
    }

    auto Shader::GetNativeHandle( renderer::rhi::ObjectType object ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( object != renderer::rhi::ObjectType::Vk_Shader ) {
            return renderer::rhi::Object{ nullptr };
        }

        return renderer::rhi::Object{ mModule };
    }

    auto Shader::GetNativeHandle( renderer::rhi::ObjectType object ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( object != renderer::rhi::ObjectType::Vk_Shader ) {
            return renderer::rhi::Object{ nullptr };
        }

        return renderer::rhi::Object( mModule );
    }

    auto Shader::GetPipelineInfo() const -> const VkPipelineShaderStageCreateInfo& {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || mModule == VK_NULL_HANDLE ||
             mStageCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO ||
             mStageCreateInfo.stage == 0 || !mStageCreateInfo.pName || mEntryPoint.empty() ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot use Vulkan shader '{}' in a pipeline: shader initialization did not succeed.", mModulePath ).c_str() );
        }

        return mStageCreateInfo;
    }

    auto Shader::GetContents() const -> const void* {
        MKT_BEGIN_PROFILER();
        return mSpirvContents.empty() ? nullptr : mSpirvContents.data();
    }

    auto Shader::GetContentsByteSize() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return MKT_VECTOR_SIZE_BYTES( mSpirvContents );
    }

    auto Shader::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        switch ( mLanguage ) {
            case renderer::rhi::ShaderLanguage::eSlang:
                CompileForSlang();
                break;
            case renderer::rhi::ShaderLanguage::eGLSL:
                CompileForGlsl();
                break;
            case renderer::rhi::ShaderLanguage::eSPIRV:
                CompileForSpirv();
                break;
            default:
                MKT_ASSERT( false, "Unsupported shader language for Vulkan Shader" );
        }

        mIsAllocated = mModule != VK_NULL_HANDLE;

        if ( !mIsAllocated ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Vulkan shader '{}' failed to initialize (entry point '{}').", mModulePath, mEntryPoint ).c_str() );
        }
    }

    auto Shader::CompileForSlang() -> void {
        MKT_BEGIN_PROFILER();
        {
            // https://docs.shader-slang.org/en/latest/compilation-api.html
            auto lease{ RenderSystem::Get()->AcquireSlangSession() };
            auto* session{ lease.GetSession( mStage ) };

            Slang::ComPtr<slang::IModule> slangModule{};
            Slang::ComPtr<slang::IBlob> spirv{};

            const eastl::string_view modulePath{ mModulePath };
            const eastl::string_view moduleName{ mModuleName };

            Slang::ComPtr<slang::IBlob> diagnosticsBlob{};

            if ( !session ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot compile Vulkan shader '{}': Slang session is unavailable.", modulePath ).c_str() );
            }
            // Compile the supplied contents when present; otherwise let Slang read
            // the path. Do not retry a failed compilation in the same session.
            if ( !mSlangContents.empty() ) {
                slangModule = session->loadModuleFromSourceString( moduleName.data(), modulePath.data(), mSlangContents.c_str(), diagnosticsBlob.writeRef() );
            }
            else {
                slangModule = session->loadModuleFromSource( moduleName.data(), modulePath.data(), nullptr, diagnosticsBlob.writeRef() );
            }

            if ( !slangModule ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Vulkan shader '{}', loading Slang module:\n{}", modulePath,
                    diagnosticsBlob ? static_cast<const char*>( diagnosticsBlob->getBufferPointer() ) : "Slang returned no diagnostics." )
                    .c_str() );
            }

            // Compose and link the requested entry point before code generation.
            // https://docs.shader-slang.org/en/latest/compilation-api.html
            Slang::ComPtr<slang::IEntryPoint> entry{};
            Slang::ComPtr<slang::IComponentType> program{};
            Slang::ComPtr<slang::IComponentType> linked{};
            if ( !SLANG_SUCCEEDED( slangModule->findEntryPointByName( mEntryPoint.c_str(), entry.writeRef() ) ) ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Vulkan shader '{}': cannot find entry point '{}'.", modulePath, mEntryPoint ).c_str() );
            }

            slang::IComponentType* components[]{ slangModule, entry };
            if ( !SLANG_SUCCEEDED( session->createCompositeComponentType( components, 2, program.writeRef(), diagnosticsBlob.writeRef() ) ) ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Vulkan shader '{}', composing program:\n{}", modulePath,
                    diagnosticsBlob ? static_cast<const char*>( diagnosticsBlob->getBufferPointer() ) : "Slang returned no diagnostics." ).c_str() );
            }
            if ( !SLANG_SUCCEEDED( program->link( linked.writeRef(), diagnosticsBlob.writeRef() ) ) ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Vulkan shader '{}', linking program:\n{}", modulePath,
                    diagnosticsBlob ? static_cast<const char*>( diagnosticsBlob->getBufferPointer() ) : "Slang returned no diagnostics." ).c_str() );
            }
            const auto compileResult{ linked->getEntryPointCode( 0, 0, spirv.writeRef(), diagnosticsBlob.writeRef() ) };
            if ( !SLANG_SUCCEEDED( compileResult ) || !spirv ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot emit SPIR-V shader bytecode." );
            }
            mSpirvContents.resize( spirv->getBufferSize() / sizeof( u32 ) );
            std::memcpy( mSpirvContents.data(), spirv->getBufferPointer(), spirv->getBufferSize() );
        }

        const auto* src{ mSpirvContents.data() };
        const auto size{ MKT_VECTOR_SIZE_BYTES( mSpirvContents ) };

#if MIKOTO_DEBUG
        // Check client specified correctly the stage
        SpvReflectShaderModule module{};
        SpvReflectResult result{ spvReflectCreateShaderModule( size, src, &module ) };

        if ( result == SPV_REFLECT_RESULT_SUCCESS ) {
            const auto* reflectedEntry{ spvReflectGetEntryPoint( &module, mEntryPoint.c_str() ) };
            const auto stage{ reflectedEntry ? reflectedEntry->shader_stage : module.shader_stage };
            VkShaderStageFlagBits moduleStage{ core::as<VkShaderStageFlagBits>( stage ) };

            if ( GetShaderModuleStage( mStage ) != moduleStage ) {
                mStage = GetShaderModuleStage( moduleStage );
                MKT_CORE_LOGGER_WARN( "Specified wrong stage for shader {}. Changed to right type.", mModulePath.c_str() );
            }

            spvReflectDestroyShaderModule( &module );
        }
#endif

        VkShaderModuleCreateInfo moduleCreateInfo{ initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = size;
        moduleCreateInfo.pCode = src;

        MKT_VK_CHECK( vkCreateShaderModule(
            core::checked_cast<Device*>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( moduleCreateInfo ),
            nullptr,
            MKT_ADDRESSOF( mModule ) ) );

        mStageCreateInfo = initializers::PipelineShaderStageCreateInfo();
        mStageCreateInfo.stage = GetShaderModuleStage( mStage );
        mStageCreateInfo.module = mModule;
        mStageCreateInfo.pName = mEntryPoint.c_str();
        mStageCreateInfo.flags = 0;
        mStageCreateInfo.pNext = nullptr;
        mStageCreateInfo.pSpecializationInfo = nullptr;

#if !defined( NDEBUG )
        // For debug purposes I store as GLSL
        mShaderCode = GetGlslFromSpirv( mSpirvContents.data(), mSpirvContents.size(), mModulePath.c_str() );
#endif

        // Cleanup
        mSlangContents.clear();
        mSlangContents.shrink_to_fit();

        mModuleName.clear();
        mModuleName.shrink_to_fit();
    }

    auto Shader::CompileForGlsl() -> void {
        MKT_BEGIN_PROFILER();

        if ( mGlslContents.empty() ) {
            MKT_CORE_LOGGER_ERROR( "Cannot compile GLSL shader '{}': source is empty.", mModulePath.c_str() );
            return;
        }

        shaderc_shader_kind shaderKind{};

        if ( !GetShaderCShaderKind( mStage, shaderKind ) ) {
            MKT_CORE_LOGGER_ERROR( "Cannot compile GLSL shader '{}': unsupported shader stage.", mModulePath.c_str() );
            return;
        }

        shaderc::Compiler compiler{};
        shaderc::CompileOptions options{};
        options.SetSourceLanguage( shaderc_source_language_glsl );
        options.SetTargetEnvironment( shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3 );
        options.SetTargetSpirv( shaderc_spirv_version_1_6 );

#if MIKOTO_DEBUG
        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel( shaderc_optimization_level_zero );
#else
        options.SetOptimizationLevel( shaderc_optimization_level_performance );
#endif

        const core::cstr sourceName{ mModulePath.empty() ? "<memory>" : mModulePath.c_str() };
        const auto compilationResult{ compiler.CompileGlslToSpv(
            mGlslContents.data(),
            mGlslContents.size(),
            shaderKind,
            sourceName,
            mEntryPoint.c_str(),
            options ) };

        if ( compilationResult.GetCompilationStatus() != shaderc_compilation_status_success ) {
            MKT_CORE_LOGGER_ERROR(
                "GLSL compilation failed for '{}':\n{}",
                sourceName,
                compilationResult.GetErrorMessage() );
            return;
        }

        mSpirvContents = eastl::vector<core::u32>{ compilationResult.cbegin(), compilationResult.cend() };

        if ( mSpirvContents.empty() ) {
            MKT_CORE_LOGGER_ERROR( "GLSL compilation produced no SPIR-V for '{}'.", sourceName );
            return;
        }

#if MIKOTO_DEBUG
        // Check client specified correctly the stage.
        SpvReflectShaderModule reflectionModule{};
        const SpvReflectResult reflectionResult{
            spvReflectCreateShaderModule( MKT_VECTOR_SIZE_BYTES( mSpirvContents ), mSpirvContents.data(), &reflectionModule )
        };

        if ( reflectionResult == SPV_REFLECT_RESULT_SUCCESS ) {
            const VkShaderStageFlagBits moduleStage{ core::as<VkShaderStageFlagBits>( reflectionModule.shader_stage ) };

            if ( GetShaderModuleStage( mStage ) != moduleStage ) {
                mStage = GetShaderModuleStage( moduleStage );
                MKT_CORE_LOGGER_WARN( "Specified wrong stage for shader {}. Changed to right type.", sourceName );
            }

            spvReflectDestroyShaderModule( &reflectionModule );
        }
#endif

        VkShaderModuleCreateInfo moduleCreateInfo{ initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = MKT_VECTOR_SIZE_BYTES( mSpirvContents );
        moduleCreateInfo.pCode = mSpirvContents.data();

        MKT_VK_CHECK( vkCreateShaderModule(
            core::checked_cast<Device*>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( moduleCreateInfo ),
            nullptr,
            MKT_ADDRESSOF( mModule ) ) );

        mStageCreateInfo = initializers::PipelineShaderStageCreateInfo();
        mStageCreateInfo.stage = GetShaderModuleStage( mStage );
        mStageCreateInfo.module = mModule;
        mStageCreateInfo.pName = mEntryPoint.c_str();
        mStageCreateInfo.flags = 0;
        mStageCreateInfo.pNext = nullptr;
        mStageCreateInfo.pSpecializationInfo = nullptr;

#if !defined( NDEBUG )
        mShaderCode = mGlslContents;
#endif

        mGlslContents.clear();
        mGlslContents.shrink_to_fit();
    }

    auto Shader::CompileForSpirv() -> void {
        MKT_BEGIN_PROFILER();
#if MIKOTO_DEBUG
        // Check client specified correctly the stage
        SpvReflectShaderModule module{};
        SpvReflectResult result{ spvReflectCreateShaderModule( MKT_VECTOR_SIZE_BYTES( mSpirvContents ), mSpirvContents.data(), &module ) };

        if ( result == SPV_REFLECT_RESULT_SUCCESS ) {
            const auto* reflectedEntry{ spvReflectGetEntryPoint( &module, mEntryPoint.c_str() ) };
            const auto stage{ reflectedEntry ? reflectedEntry->shader_stage : module.shader_stage };
            VkShaderStageFlagBits moduleStage{ core::as<VkShaderStageFlagBits>( stage ) };

            if ( GetShaderModuleStage( mStage ) != moduleStage ) {
                mStage = GetShaderModuleStage( moduleStage );
                MKT_CORE_LOGGER_WARN( "Specified wrong stage for shader {}. Changed to right type.", mModulePath.c_str() );
            }

            spvReflectDestroyShaderModule( &module );
        }
#endif

        VkShaderModuleCreateInfo moduleCreateInfo{ initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = MKT_VECTOR_SIZE_BYTES( mSpirvContents );
        moduleCreateInfo.pCode = core::as<core::u32*>( mSpirvContents.data() );

        MKT_VK_CHECK( vkCreateShaderModule(
            core::checked_cast<Device*>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( moduleCreateInfo ),
            nullptr,
            MKT_ADDRESSOF( mModule ) ) );

        mStageCreateInfo = initializers::PipelineShaderStageCreateInfo();
        mStageCreateInfo.stage = GetShaderModuleStage( mStage );
        mStageCreateInfo.module = mModule;
        mStageCreateInfo.pName = mEntryPoint.c_str();
        mStageCreateInfo.flags = 0;
        mStageCreateInfo.pNext = nullptr;
        mStageCreateInfo.pSpecializationInfo = nullptr;

#if !defined( NDEBUG )
        // For debug purposes I store as GLSL
        mShaderCode = GetGlslFromSpirv( mSpirvContents.data(), mSpirvContents.size(), mModulePath.c_str() );
#endif
    }

    auto Shader::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        vkDestroyShaderModule( core::checked_cast<Device*>( mDevice )->GetDevice(), mModule, nullptr );
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
}