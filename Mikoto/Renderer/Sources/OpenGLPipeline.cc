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

#include <EASTL/utility.h>

#include <glm/glm.hpp>

#include <Core/Exception.hh>

#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>
#include <Renderer/Rhi/OpenGL/OpenGLTexture.hh>
#include <Renderer/Rhi/OpenGL/OpenGLPipeline.hh>

namespace mikoto::renderer::opengl {

    auto Pipeline::GetPipelineType() const -> rhi::PipelineType {
        return mPipelineType;
    }

    Pipeline::Pipeline( const renderer::rhi::GraphicsPipelineDescription &desc, eastl::shared_ptr<DeviceState> state )
        : mPipelineType{ renderer::rhi::PipelineType::eGraphics },
          mGraphics{ desc },
          mState{ eastl::move( state ) } {
    }

    Pipeline::Pipeline( const renderer::rhi::ComputePipelineDescription &desc, eastl::shared_ptr<DeviceState> state )
        : mPipelineType{ renderer::rhi::PipelineType::eCompute },
          mCompute{ desc },
          mState{ eastl::move( state ) } {
    }

    Pipeline::~Pipeline() {
        Destroy();
    }

    auto Pipeline::Initialize() -> void {
        mState->RequireOwner();

        if ( mGraphics.mUseReflection || mCompute.mUseReflection ) {
            Unsupported( "automatic pipeline layout reflection" );
        }

        mProgram = glCreateProgram();
        const auto attach{ [&]( const renderer::rhi::ShaderModuleHandle &shader ) {
            const auto *glShader{ dynamic_cast<const Shader *>( shader.operator->() ) };

            if ( !glShader ) {
                MKT_THROW_INVALID_ARGUMENT( "Pipeline requires OpenGL shader modules." );
            }

            glAttachShader( mProgram, glShader->Name() );

            for ( const auto &binding: glShader->GetCombinedSamplers() ) {
                bool present{};

                for ( const auto &existing: mCombinedSamplers ) {
                    present |= existing.mName == binding.mName;
                }

                if ( !present ) {
                    mCombinedSamplers.push_back( binding );
                }
            }
        } };

        if ( mPipelineType == renderer::rhi::PipelineType::eCompute ) {

            if ( !mCompute.mStage || mCompute.mStage->GetType() != renderer::rhi::ShaderType::eCompute ) {
                MKT_THROW_INVALID_ARGUMENT( "Compute pipelines require a compute shader." );
            }

            attach( mCompute.mStage );
        }
        else {

            if ( mGraphics.mEnableStencilTest ) {
                Unsupported( "stencil pipelines without explicit stencil operations in the RHI" );
            }

            if ( !mGraphics.mShaders.contains( renderer::rhi::ShaderType::eVertex ) ) {
                MKT_THROW_INVALID_ARGUMENT( "Graphics pipelines require a vertex shader." );
            }

            for ( const auto &[stage, shader]: mGraphics.mShaders ) {

                if ( stage == renderer::rhi::ShaderType::eCompute ) {
                    MKT_THROW_INVALID_ARGUMENT( "A graphics pipeline cannot contain a compute shader." );
                }

                if ( stage == renderer::rhi::ShaderType::eHull || stage == renderer::rhi::ShaderType::eDomain ) {
                    Unsupported( "tessellation pipelines without patch topology in the RHI" );
                }

                attach( shader );
            }
        }

        glLinkProgram( mProgram );
        GLint linked{};
        glGetProgramiv( mProgram, GL_LINK_STATUS, &linked );

        if ( !linked ) {
            GLint length{};
            glGetProgramiv( mProgram, GL_INFO_LOG_LENGTH, &length );
            eastl::string log( glm::max( length, 1 ), '\0' );
            glGetProgramInfoLog( mProgram, static_cast<GLsizei>( log.size() ), nullptr, log.data() );
            MKT_THROW_RUNTIME_ERROR( "OpenGL program link failed: " + log );
        }

        if ( !mCombinedSamplers.empty() ) {
            GLint limit{};
            glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &limit );
            eastl::vector<bool> used( static_cast<core::usize>( limit ), false );
            auto *layout{ dynamic_cast<PipelineLayout *>( GetPipelineLayout().GetPtr() ) };

            if ( !layout ) {
                MKT_THROW_INVALID_ARGUMENT( "Combined samplers require an explicit OpenGL pipeline layout." );
            }

            // Preserve the explicit units used by native GLSL and by ordinary
            // descriptor bindings. Synthetic pairs use the remaining units.

            for ( const auto &handle: layout->GetDescription().mBindingLayouts ) {
                const auto *tableLayout{ dynamic_cast<const BindingLayout *>( handle.GetPtr() ) };

                for ( const auto &item: tableLayout->GetDescription().mBindings ) {

                    if ( item.mType == renderer::rhi::ResourceType::eTexture_SRV || item.mType == renderer::rhi::ResourceType::eSampler ) {

                        if ( item.mSlot >= used.size() ) {
                            MKT_THROW_OUT_OF_RANGE( "Texture or sampler slot exceeds OpenGL's texture unit limit." );
                        }

                        used[item.mSlot] = true;
                    }
                }
            }

            for ( auto &binding: mCombinedSamplers ) {
                core::usize unit{};

                while ( unit < used.size() && used[unit] ) {
                    ++unit;
                }

                if ( unit == used.size() ) {
                    MKT_THROW_OUT_OF_RANGE( "Combined texture/sampler pairs exceed OpenGL's texture unit limit." );
                }

                used[unit] = true;
                binding.mUnit = static_cast<GLuint>( unit );
                glProgramUniform1i( mProgram, glGetUniformLocation( mProgram, binding.mName.c_str() ), binding.mUnit );
            }
        }

        CheckError( "pipeline creation" );
        mIsAllocated = true;
    }

    auto Pipeline::GetPipelineLayout() const -> renderer::rhi::PipelineLayoutHandle {
        return mPipelineType == renderer::rhi::PipelineType::eGraphics ? mGraphics.mPipelineLayout : mCompute.mPipelineLayout;
    }

    auto Pipeline::BindCombinedSamplers( const BindingTableSnapshots &tables ) const -> void {
        const auto find{ [&]( renderer::rhi::ResourceType type, core::u32 set, core::u32 slot ) -> const renderer::rhi::BindingTableItem * {
            for ( const auto &[boundSet, table]: tables ) {

                if ( boundSet == set ) {
                    return table->Find( type, slot );
                }
            }

            return nullptr;
        } };

        for ( const auto &binding: mCombinedSamplers ) {
            const auto *image{ find( renderer::rhi::ResourceType::eTexture_SRV, binding.mTextureSet, binding.mTextureSlot ) };
            const auto *texture{ image ? dynamic_cast<const Texture *>( image->mResource ) : nullptr };

            if ( !texture ) {
                MKT_THROW_INVALID_ARGUMENT( "Missing texture binding for an OpenGL combined sampler." );
            }

            GLuint samplerName{};

            if ( !binding.mDummySampler ) {
                const auto *item{ find( renderer::rhi::ResourceType::eSampler, binding.mSamplerSet, binding.mSamplerSlot ) };
                const auto *sampler{ item ? dynamic_cast<const Sampler *>( item->mResource ) : nullptr };

                if ( !sampler ) {
                    MKT_THROW_INVALID_ARGUMENT( "Missing sampler binding for an OpenGL combined sampler." );
                }

                samplerName = sampler->Name();
            }

            glBindTextureUnit( binding.mUnit, texture->SampledView( *image ) );
            glBindSampler( binding.mUnit, samplerName );
        }
    }

    auto Pipeline::Topology() const -> GLenum {
        return opengl::GetTopology( mGraphics.mPrimitiveTopology );
    }

    auto Pipeline::Bind( GLuint vao ) const -> void {
        glUseProgram( mProgram );

        if ( mPipelineType == renderer::rhi::PipelineType::eCompute ) {
            return;
        }

        glBindVertexArray( vao );

        // Reapply depth and raster state because the preceding pass may use a different pipeline.
        opengl::SetCapability( GL_DEPTH_TEST, mGraphics.mEnableDepthTest );
        glDepthMask( mGraphics.mEnableDepthWrite );
        glDepthFunc( opengl::GetCompareOp( mGraphics.mDepthCompareOp ) );

        opengl::SetCapability( GL_CULL_FACE, mGraphics.mCullMode != renderer::rhi::CullMode::eNone );
        glCullFace( opengl::GetCullMode( mGraphics.mCullMode ) );
        glFrontFace( opengl::GetWindingOrder( mGraphics.mWindingOrder ) );
        glPolygonMode( GL_FRONT_AND_BACK, opengl::GetPolygonMode( mGraphics.mPolygonMode ) );
        glLineWidth( mGraphics.mPolygonLineWidth );

        // Keep colour blending and channel writes consistent with the RHI pipeline description.
        const auto &blend{ mGraphics.mAlphaBlendState };
        opengl::SetCapability( GL_BLEND, blend.mEnabled );
        glBlendEquationSeparate( opengl::GetBlendOp( blend.mColorOperation ), opengl::GetBlendOp( blend.mAlphaOperation ) );
        glBlendFuncSeparate( opengl::GetBlendFactor( blend.mSrcColorFactor ), opengl::GetBlendFactor( blend.mDstColorFactor ),
            opengl::GetBlendFactor( blend.mSrcAlphaFactor ), opengl::GetBlendFactor( blend.mDstAlphaFactor ) );
        const auto writeMask{ opengl::GetColorWriteMask( blend.mWriteMask ) };
        glColorMask( writeMask[0], writeMask[1], writeMask[2], writeMask[3] );

        opengl::SetCapability( GL_SAMPLE_SHADING, mGraphics.mEnableSampleRateShading );
        glMinSampleShading( 1.0f );
        glDisable( GL_STENCIL_TEST );

        if ( mGraphics.mInputLayout ) {
            const auto *input{ dynamic_cast<const InputLayout *>( mGraphics.mInputLayout.operator->() ) };

            if ( !input ) {
                MKT_THROW_INVALID_ARGUMENT( "Expected OpenGL input layout." );
            }

            input->Apply( vao );
        }
        else {
            GLint count{};
            glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &count );

            for ( GLint index{}; index < count; ++index ) {
                glDisableVertexArrayAttrib( vao, index );
            }
        }
    }

    auto Pipeline::Destroy() -> void {
        if ( mProgram ) {
            const auto program{ eastl::exchange( mProgram, 0 ) };
            mState->Retire( [program] {
                glDeleteProgram( program );
            } );
        }

        mIsAllocated = false;
    }
}// namespace mikoto::renderer::opengl
