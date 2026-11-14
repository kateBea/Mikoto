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
#include <string>
#include <climits>
#include <cstring>

#include <EASTL/utility.h>

#include <glm/glm.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

#include <spirv_glsl.hpp>

#include <Core/Exception.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/OpenGL/OpenGLShader.hh>
#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>

namespace mikoto::renderer::opengl {

    auto Shader::GetType() const -> rhi::ShaderType {
        return mStage;
    }

    auto Shader::DumpShaderCode() -> void {
    }

    auto Shader::Name() const -> GLuint {
        return mName;
    }

    auto Shader::GetCombinedSamplers() const -> const eastl::vector<CombinedSamplerBinding> & {
        return mCombinedSamplers;
    }

    Shader::Shader( const renderer::rhi::ShaderModuleCreateDescription &desc, eastl::shared_ptr<DeviceState> state )
        : mEntryPoint{ desc.mEntryPoint }, mStage{ desc.mType }, mLanguage{ desc.mLanguage }, mState{ eastl::move( state ) }, mByteSize{ desc.mShaderContentsSize }, mModuleName{ desc.mModuleName }, mModulePath{ desc.mModulePath } {

        if ( !desc.mShaderContents || !mByteSize || mByteSize > INT_MAX ) {
            MKT_THROW_INVALID_ARGUMENT( "Empty or oversized OpenGL shader." );
        }

        mCode.resize( ( mByteSize + 3 ) / 4 );
        std::memcpy( mCode.data(), desc.mShaderContents, mByteSize );
    }

    Shader::~Shader() {
        Destroy();
    }

    auto Shader::GetContents() const -> const void * {
        return mCode.data();
    }

    auto Shader::GetContentsByteSize() const -> core::usize {
        return mByteSize;
    }

    auto Shader::Initialize() -> void {
        mState->RequireOwner();
        const auto type{ opengl::GetShaderModuleStage( mStage ) };

        if ( mLanguage == renderer::rhi::ShaderLanguage::eSlang ) {
            auto *renderer{ RenderSystem::GetPtr() };

            if ( !renderer || !renderer->IsApiActive( renderer::rhi::GraphicsAPI::eOpenGL ) ) {
                MKT_THROW_LOGIC_ERROR( "Runtime Slang compilation requires the OpenGL RenderSystem session." );
            }

            {
                auto lease{ renderer->AcquireSlangSession() };
                auto *session{ lease.GetSession( mStage ) };
                Slang::ComPtr<slang::IBlob> diagnostics{};
                const auto require{ [&]( SlangResult result ) {
                    if ( SLANG_FAILED( result ) ) {
                        const eastl::string message{ diagnostics ? static_cast<const char *>( diagnostics->getBufferPointer() )
                                                                 : "No compiler diagnostics." };
                        MKT_THROW_RUNTIME_ERROR( "Slang OpenGL compilation failed: " + message );
                    }
                } };
                const eastl::string source{ reinterpret_cast<const char *>( mCode.data() ), mByteSize };
                Slang::ComPtr<slang::IModule> module{ session->loadModuleFromSourceString(
                    mModuleName.empty() ? "OpenGLShader" : mModuleName.c_str(), mModulePath.c_str(), source.c_str(),
                    diagnostics.writeRef() ) };

                if ( !module ) {
                    require( SLANG_FAIL );
                }

                Slang::ComPtr<slang::IEntryPoint> entry{};
                require( module->findEntryPointByName( mEntryPoint.c_str(), entry.writeRef() ) );
                const std::array<slang::IComponentType *, 2> components{ module.get(), entry.get() };
                Slang::ComPtr<slang::IComponentType> program{};
                Slang::ComPtr<slang::IComponentType> linked{};
                require( session->createCompositeComponentType( components.data(), core::as<SlangInt>( components.size() ), program.writeRef(), diagnostics.writeRef() ) );
                require( program->link( linked.writeRef(), diagnostics.writeRef() ) );
                Slang::ComPtr<slang::IBlob> code{};
                require( linked->getEntryPointCode( 0, 0, code.writeRef(), diagnostics.writeRef() ) );
                mByteSize = code->getBufferSize();
                mCode.resize( ( mByteSize + 3 ) / 4 );
                std::memcpy( mCode.data(), code->getBufferPointer(), mByteSize );
            }

            spirv_cross::CompilerGLSL compiler{ mCode.data(), mCode.size() };
            auto options{ compiler.get_common_options() };
            options.version = 460;
            options.es = false;
            options.vulkan_semantics = false;
            options.emit_push_constant_as_uniform_buffer = true;
            compiler.set_common_options( options );
            const auto resources{ compiler.get_shader_resources() };

            for ( const auto &resource: resources.push_constant_buffers ) {
                compiler.set_decoration( resource.id, spv::DecorationBinding, PipelineLayout::GetPushConstantBinding() );
                compiler.unset_decoration( resource.id, spv::DecorationDescriptorSet );
            }

            // A combined sampler array cannot represent arbitrary independent
            // texture/sampler array indexing. Keep that unsupported explicitly.

            for ( const auto &list: { resources.separate_images, resources.separate_samplers } ) {

                for ( const auto &resource: list ) {

                    if ( !compiler.get_type( resource.type_id ).array.empty() ) {
                        Unsupported( "separate texture/sampler arrays in Slang shaders" );
                    }
                }
            }

            const auto dummySampler{ compiler.build_dummy_sampler_for_combined_images() };
            compiler.build_combined_image_samplers();

            for ( const auto &pair: compiler.get_combined_image_samplers() ) {
                CombinedSamplerBinding binding{};
                binding.mTextureSlot = compiler.get_decoration( pair.image_id, spv::DecorationBinding );
                binding.mTextureSet = compiler.get_decoration( pair.image_id, spv::DecorationDescriptorSet );
                binding.mDummySampler = pair.sampler_id == dummySampler;

                if ( !binding.mDummySampler ) {
                    binding.mSamplerSlot = compiler.get_decoration( pair.sampler_id, spv::DecorationBinding );
                    binding.mSamplerSet = compiler.get_decoration( pair.sampler_id, spv::DecorationDescriptorSet );
                }

                // Stable names let identical pairs share a uniform across stages.
                const std::string name{ "mkt_combined_t" + std::to_string( binding.mTextureSet ) + "_" +
                                        std::to_string( binding.mTextureSlot ) + "_s" +
                                        ( binding.mDummySampler ? "dummy" : std::to_string( binding.mSamplerSet ) + "_" + std::to_string( binding.mSamplerSlot ) ) };
                binding.mName = name.c_str();
                compiler.set_name( pair.combined_id, name );
                // The linked pipeline assigns a free texture unit to each pair.
                compiler.unset_decoration( pair.combined_id, spv::DecorationBinding );
                compiler.unset_decoration( pair.combined_id, spv::DecorationDescriptorSet );
                mCombinedSamplers.push_back( eastl::move( binding ) );
            }

            const auto glsl{ compiler.compile() };
            mByteSize = glsl.size();
            mCode.resize( ( mByteSize + 3 ) / 4 );
            std::memcpy( mCode.data(), glsl.data(), mByteSize );
            mLanguage = renderer::rhi::ShaderLanguage::eGLSL;
            mEntryPoint = "main";
        }

        if ( mLanguage != renderer::rhi::ShaderLanguage::eGLSL && mLanguage != renderer::rhi::ShaderLanguage::eSPIRV ) {
            Unsupported( "uncompiled Slang/HLSL shaders; provide GLSL 460 or OpenGL-targeted SPIR-V" );
        }

        if ( mLanguage == renderer::rhi::ShaderLanguage::eSPIRV ) {

            if ( mByteSize % 4 || mCode.size() < 5 || mCode[0] != 0x07230203 ) {
                MKT_THROW_INVALID_ARGUMENT( "Invalid SPIR-V header." );
            }

            for ( core::usize offset{ 5 }; offset < mCode.size(); ) {
                const core::u32 length{ mCode[offset] >> 16 };
                const core::u32 opcode{ mCode[offset] & 0xffff };

                if ( !length || length > mCode.size() - offset ) {
                    MKT_THROW_INVALID_ARGUMENT( "SPIR-V instruction goes out of bounds." );
                }

                // OpMemoryModel: OpenGL SPIR-V requires Logical addressing.

                if ( opcode == 14 && length >= 3 && mCode[offset + 1] != 0 ) {
                    Unsupported( "physical buffer pointers in Vulkan SPIR-V" );
                }

                offset += length;
            }
        }

        mName = glCreateShader( type );

        if ( mLanguage == renderer::rhi::ShaderLanguage::eGLSL ) {

            if ( mEntryPoint != "main" ) {
                Unsupported( "GLSL entry points other than main" );
            }

            const auto source{ static_cast<const GLchar *>( GetContents() ) };
            const GLint size{ static_cast<GLint>( mByteSize ) };
            glShaderSource( mName, 1, &source, &size );
            glCompileShader( mName );
        }
        else {
            glShaderBinary( 1, &mName, GL_SHADER_BINARY_FORMAT_SPIR_V, mCode.data(), static_cast<GLsizei>( mByteSize ) );
            glSpecializeShader( mName, mEntryPoint.c_str(), 0, nullptr, nullptr );
        }

        GLint compiled{};
        glGetShaderiv( mName, GL_COMPILE_STATUS, &compiled );

        if ( !compiled ) {
            GLint length{};
            glGetShaderiv( mName, GL_INFO_LOG_LENGTH, &length );
            eastl::string log( glm::max( length, 1 ), '\0' );
            glGetShaderInfoLog( mName, static_cast<GLsizei>( log.size() ), nullptr, log.data() );
            MKT_THROW_RUNTIME_ERROR( "OpenGL shader compilation failed: " + log );
        }

        CheckError( "shader creation" );
        mIsAllocated = true;
    }

    auto Shader::Destroy() -> void {
        if ( mName ) {
            const auto name{ eastl::exchange( mName, 0 ) };
            mState->Retire( [name] {
                glDeleteShader( name );
            } );
        }

        mIsAllocated = false;
    }
}// namespace mikoto::renderer::opengl
