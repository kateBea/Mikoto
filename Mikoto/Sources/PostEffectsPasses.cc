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

#include <Core/Profiler.hh>
#include <Common/String.hh>
#include <Library/Random/Random.hh>
#include <Library/String/String.hh>
#include <Math/Math.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Passes/PostEffectsPasses.hh>
#include <Renderer/Passes/ShaderParameteres.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    PostEffectsPass::PostEffectsPass( RenderResolution resolution )
        : m_Resolution{ resolution } {
    }

    auto PostEffectsPass::SetScene( const Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto PostEffectsPass::SetCamera( const Camera* camera ) -> void {
        m_InfiniteGridParameters.CameraPos = Vec4F{ camera->GetPosition(), 1.0f };
        m_InfiniteGridParameters.CameraView = camera->GetViewMatrix();
        m_InfiniteGridParameters.CameraProj = camera->GetProjection();

        m_Camera = camera;
    }

    auto PostEffectsPass::RegisterPasses( FrameGraph& graph, GpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterBloom( graph );
        RegisterSSAO( graph );
        RegisterTonemap( graph );
        RegisterGradient( graph );
        RegisterPostProcessMaterialsFilter( graph );
        RegisterChromaticAberration( graph );
        RegisterTextRender( graph, device );
        RegisterObjectOutline( graph );
    }

    auto PostEffectsPass::SetContrast( float contrast ) -> void {
        m_ColorGradientParameters.Contrast = contrast;
    }

    auto PostEffectsPass::SetSaturation( float saturation ) -> void {
        m_ColorGradientParameters.Saturation = saturation;
    }

    auto PostEffectsPass::SetTintColor( const Vec3F& tintColor ) -> void {
        m_ColorGradientParameters.Tint = tintColor;
    }

    auto PostEffectsPass::EnableBloom( bool value ) -> void {

    }

    auto PostEffectsPass::SetMeshCulling( MeshCulling& culling ) -> void {
        m_MeshCullingPass = std::addressof( culling );
    }

    auto PostEffectsPass::RegisterTextRender( FrameGraph& graph, GpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        m_TextInfo.resize( MAX_GLYPHS );

        // Vertex buffer and index buffer for the quads
        BufferDescription vertexDesc{};
        vertexDesc.WithData( reinterpret_cast<Byte*>( VERTICES.data() ) )
                .WithUsage( BufferUsage::VERTEX )
                .WithSizeBytes( InferSize<FontVertex>( VERTICES.size() ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        m_TextVertexBuffer = device->CreateBuffer( vertexDesc );

        BufferDescription indexDesc{};
        indexDesc.WithData( reinterpret_cast<Byte*>( INDICES.data() ) )
                .WithUsage( BufferUsage::INDEX )
                .WithSizeBytes( InferSize<UInt32>( INDICES.size() ) )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        m_TextIndexBuffer = device->CreateBuffer( indexDesc );
        
        graph.RegisterPass(
            "MSDFTextPass",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
                
                b.CreateBuffer( "MSDFTextPass_TextData", BufferUsage::SHADER_STORAGE, 
                    MKT_SIZEOF( TextRenderParams ), MAX_GLYPHS, ResourceUsageType::RESOURCE_USAGE_DYNAMIC );
               
                b.UseShader( "Resources/Shaders/slang/MSDFText_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/MSDFText_Frag.slang", ShaderStage::FRAGMENT );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .AlphaBlending{ true },
                };
                
                // we're providing none as the shader expects none
                const BufferLayout layout{
                    { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                    { ShaderDataType::UINT_TYPE, "a_TexCoordIndex" },
                };
                graphicsDesc.VertexAttributesSpec = { AttributesSpec{
                        .DefaultVertexLayout{ layout },
                        .InputRateSpec{ .BindingIndex = { 0 }, .AttributeRate{ InputRate::PER_VERTEX } } } };
                graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;
                graphicsDesc.ColorAttachmentFormats = { TextureFormat::RGBA16_FLOAT };
                b.CreatePipeline( "TextRenderPass_Pipeline", graphicsDesc );

                b.Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "InfiniteGrid_BufferEDGE+", FrameResourceState::UniformBuffer );

                b.Write( "3DRenderTextEdge", FrameResourceState::UniformBuffer );
                b.Write( "MSDFTextPass_TextData", FrameResourceState::UnorderedAccessView );
            },
            [this]( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                MKT_ASSERT( m_Scene != nullptr, "Scene cannot be NULL" );

                TraverseTextList( ctx );

                if (m_GlyphCount == 0) {
                    return;
                }

                ctx.CopyBuffer( "MSDFTextPass_TextData", m_TextInfo.data(), sizeof( TextRenderParams ) * m_GlyphCount );

                ctx.SetColorRenderTarget( "FinalShadingPass_ColorTarget" );
                ctx.SetDepthRenderTarget( "FinalShadingPass_DepthTarget" );

                PassRenderInfo renderInfo{
                    .ColorLoadOp{ LoadOp::LOAD },
                    .DephtLoadOp{ LoadOp::LOAD }
                };

                ctx.BeginRender( renderInfo );

                ctx.BindGroup( ResourceGroup::UnboundedImageViews, "Texture2D_List" );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MSDFTextPass_TextData", ResourceSlot::Slot_0 );

                ctx.BindPipeline( "TextRenderPass_Pipeline" );

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                DrawIndexedState drawIndexedState{};

                drawIndexedState.IndexBuffer = m_TextIndexBuffer;
                drawIndexedState.VertexBuffers.emplace_back( m_TextVertexBuffer, 0 );

                drawIndexedState.IndicesCount = m_TextIndexBuffer->GetCount();
                drawIndexedState.InstancesCount = m_GlyphCount;

                ctx.DrawIndexed( drawIndexedState );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterObjectOutline( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // First pass here will require the positions
        // So it will probably depend on GBuffer_Positions

        // Filter objects that mush appear outlined
        graph.RegisterPass(
            "OutlineObjectsFilter",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );

        graph.RegisterPass(
            "OutlineMask",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );

        graph.RegisterPass(
            "OutlineSeedInit",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );

        graph.RegisterPass(
            "JFA_Resolution",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );

        graph.RegisterPass(
            "CompositeOutline",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );
    }

    auto PostEffectsPass::RegisterChromaticAberration( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "ChromaticAberration",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "ChromaticAberration_ColorTarget", m_Resolution, TextureFormat::RGBA16_FLOAT, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/ChromaticAberration_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/ChromaticAberration_Frag.slang", ShaderStage::FRAGMENT );

                b.CreatePipeline( "ChromaticAberration_Pipeline",
                    GraphicsPipelineDescription{
                    .VertexAttributesSpec{},
                    .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                } );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
                b.Read( "FinalShadingPass_BufferEDGE+", FrameResourceState::UniformBuffer );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "FinalShadingPass_ColorTarget", ResourceSlot::Slot_0 );

                ctx.SetColorRenderTarget( "ChromaticAberration_ColorTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "ChromaticAberration_Pipeline" );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterDepthOfField( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Pending:
        // https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-28-practical-post-process-depth-field
        graph.RegisterPass(
            "DepthOfField_InitializeCoC",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );

        graph.RegisterPass(
            "DepthOfField_ComputeNearCoC",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );

        graph.RegisterPass(
            "DepthOfField_BlurNearCoC",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );
    }

    auto PostEffectsPass::RegisterSSAO( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Generate noise
        constexpr UInt32 SSAO_NOISE_DIM{ 8 };
        for ( UInt32 i{}; i < SSAO_NOISE_DIM * SSAO_NOISE_DIM; i++ ) {
            // rotate around z-axis (in tangent space)
            m_SSONoise.emplace_back( GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, 0.0f, 0.0f );
        }

        // Generate samples
        for ( UInt32 i{}; i < m_SSAOParameters.Samples.size(); ++i ) {
            Vec4F sample( GetRandomReal( 0.0, 1.0 ) * 2.0 - 1.0, GetRandomReal( 0.0, 1.0 ) * 2.0 - 1.0, GetRandomReal( 0.0, 1.0 ), 1.0 );
            sample = glm::normalize( sample );
            sample *= GetRandomReal( 0.0, 1.0 );
            float scale{ static_cast<float>( i ) / 64.0f };

            // scale samples s.t. they're more aligned to center of kernel
            scale = Math::Lerp( 0.1f, 1.0f, scale * scale );
            sample *= scale;
            m_SSAOParameters.Samples[i] = sample;
        }

        graph.RegisterPass(
            "SSAO",
            [this]( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateBuffer( "SSAO_Parameters", BufferUsage::UNIFORM, sizeof( m_SSAOParameters ), 1, ResourceUsageType::RESOURCE_USAGE_STREAMING );

                b.CreateTexture( "SSAO_ColorTarget", m_Resolution, TextureFormat::R8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "SSAO_NoiseTexture", SSAO_NOISE_DIM, SSAO_NOISE_DIM, // SSAO_NOISE_DIM is constexpr, capturing it is not required
                                 TextureFormat::RGBA32_FLOAT, m_SSONoise.data(), MKT_SIZEOF( Vec4F ) * m_SSONoise.size() );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::R8_UNORM }
                };

                b.UseShader( "Resources/Shaders/slang/SSAO_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/SSAO_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "SSAO_Pipeline", graphicsDesc );

                b.Write( "SSAO_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "SSAO_NoiseTexture", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Read( "SSAO_Parameters", FrameResourceState::UniformBuffer );
                b.Read( "GBuffer_Position", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "GBuffer_Normal", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext& ctx, FrameGraphBlackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                auto& constants{ blackboard.Get<FinalShadingConstants>() };
                if (!constants.EnableSSAO) {
                    return;
                }

                if ( m_Sampler.IsEmpty() || m_SamplerNoise.IsEmpty() ) {
                    m_Sampler = ctx.CreateSampler( SamplerDescription{} );

                    m_SamplerNoise = ctx.CreateSampler( SamplerDescription{
                            .MipLevels{ 0 },
                            .MinFilter{ SamplerFilter::FILTER_NEAREST },
                            .MagFilter{ SamplerFilter::FILTER_NEAREST },

                            .WrapU{ SamplerWrapMode::WRAP_REPEAT },
                            .WrapV{ SamplerWrapMode::WRAP_REPEAT },
                            .WrapW{ SamplerWrapMode::WRAP_REPEAT },
                    } );
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "SSAO_Parameters", ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Position", m_Sampler, ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Normal", m_Sampler, ResourceSlot::Slot_1 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "SSAO_NoiseTexture", m_SamplerNoise, ResourceSlot::Slot_2 );
                
                m_SSAOParameters.Projection = m_Camera->GetProjection();

                ctx.UploadBuffer( "SSAO_Parameters", m_SSAOParameters );

                ctx.SetColorRenderTarget( "SSAO_ColorTarget" );

                ctx.BeginRender();

                const auto dimensions{ InferDimensions( m_Resolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindPipeline( "SSAO_Pipeline" );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );

        graph.RegisterPass(
                "SSAOBlur",
                [this]( FramePassBuilder& b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.CreateTexture( "SSAOBlur_ColorTarget", m_Resolution, TextureFormat::R8_UNORM, TextureUsage::COLOR );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ false },
                        .DepthWrite{ false },
                        .ColorAttachmentFormats{ TextureFormat::R8_UNORM },
                    };

                    b.UseShader( "Resources/Shaders/slang/SSAOBlur_Vert.slang", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/slang/SSAOBlur_Frag.slang", ShaderStage::FRAGMENT );
                    b.CreatePipeline( "SSAOBlur_Pipeline", graphicsDesc );

                    b.Write( "SSAOBlur_ColorTarget", FrameResourceState::RenderTarget );
                    b.Read( "SSAO_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                },
                [this]( CommandContext& ctx, FrameGraphBlackboard& blackboard ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    auto& constants{ blackboard.Get<FinalShadingConstants>() };
                    if ( !constants.EnableSSAO ) {
                        return;
                    }

                    if ( m_Sampler.IsEmpty() ) {
                        m_Sampler = ctx.CreateSampler( SamplerDescription{} );
                    }

                    ctx.BindImageSampler( ResourceGroup::StaticSamplers, "SSAO_ColorTarget", m_Sampler, ResourceSlot::Slot_0 );

                    ctx.SetColorRenderTarget( "SSAOBlur_ColorTarget" );

                    ctx.BeginRender();

                    const auto dimensions{ InferDimensions( m_Resolution ) };
                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    ctx.BindPipeline( "SSAOBlur_Pipeline" );
                    ctx.Draw( 3 );

                    ctx.EndRender();
                } );
    }

    auto PostEffectsPass::RegisterBloom( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for (UInt32 mipLevel{ 0 }; mipLevel < 4; ++mipLevel ) {
            auto dimensions{ InferDimensions( m_Resolution ) };
            float width{  static_cast<float>(dimensions.first * std::pow(0.5f, mipLevel)) };
            float height{  static_cast<float>(dimensions.second * std::pow(0.5f, mipLevel)) };

            graph.RegisterPass(
            mipLevel == 0 ?
            fmt::format( "BloomDownSampling Emissive to 0" ) :
            fmt::format( "BloomDownSampling Mip {} to {}", mipLevel, mipLevel - 1 ),
            [this, mipLevel, width, height]( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();
                b.CreateTexture( fmt::format( "BloomDownSampling_ColorTarget_{}", mipLevel ), width, height, TextureFormat::RGBA16_FLOAT, Multisampling::MSAA_X1, TextureUsage::COLOR );

                b.CreateBuffer( "Bloom_Parameters", BufferUsage::UNIFORM, sizeof( BloomParameters ), 1, ResourceUsageType::RESOURCE_USAGE_STREAMING );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                };

                b.UseShader( "Resources/Shaders/slang/BloomDown_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/BloomDown_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( fmt::format( "BloomDownSamplingPipeline_{}", mipLevel ), graphicsDesc );

                b.Write( "Bloom_Parameters", FrameResourceState::UniformBuffer );
                b.Write( fmt::format( "BloomDownSampling_ColorTarget_{}", mipLevel ), FrameResourceState::RenderTarget );

                if (mipLevel == 0) {
                    b.Read( "GBuffer_Emissive", FrameResourceState::ShaderRead_GraphicsPipeline );
                } else {
                    b.Read( fmt::format( "BloomDownSampling_ColorTarget_{}", mipLevel - 1 ), FrameResourceState::ShaderRead_GraphicsPipeline );
                }

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
            },
            [this, mipLevel, width, height]( CommandContext& ctx, FrameGraphBlackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                if (!blackboard.Contains<BloomParameters>()) {
                    blackboard.Add<BloomParameters>();
                }

                auto& data{ blackboard.Get<BloomParameters>() };

                if (data.GBufferEmissiveSampler.IsEmpty() ) {
                    SamplerDescription description{
                        .MinFilter{ SamplerFilter::FILTER_LINEAR },
                        .MagFilter{ SamplerFilter::FILTER_LINEAR },
                        .WrapU{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE },
                        .WrapV{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE },
                        .WrapW{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE },
                    };

                    data.GBufferEmissiveSampler = ctx.CreateSampler( description );
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );

                if (mipLevel == 0) {
                    ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Emissive", data.GBufferEmissiveSampler, ResourceSlot::Slot_0 );
                } else {
                    ctx.BindImageSampler( ResourceGroup::StaticSamplers, fmt::format( "BloomDownSampling_ColorTarget_{}", mipLevel - 1 ), data.GBufferEmissiveSampler, ResourceSlot::Slot_0 );
                }

                ctx.SetColorRenderTarget( fmt::format( "BloomDownSampling_ColorTarget_{}", mipLevel ) );

                PassRenderInfo info{ .ColorLoadOp{LoadOp::LOAD} };
                ctx.BeginRender(info);

                ctx.SetViewport( 0, 0, width, height, false );
                ctx.SetScissor( 0, 0, width, height );

                ctx.BindPipeline( fmt::format( "BloomDownSamplingPipeline_{}", mipLevel ) );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
        }

#if false
        graph.RegisterPass(
            "BloomUpSampling",
            []( FramePassBuilder& b ) {
                MKT_BEGIN_PROFILER_NAMED();

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                };

                b.UseShader( "Resources/Shaders/slang/BloomUp_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/BloomUp_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "BloomUpSampling_Pipeline", graphicsDesc );

                b.Read( "Bloom_Parameters", FrameResourceState::UniformBuffer );
                b.Read( "BloomDownSampling_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "GBuffer_Emissive", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext& ctx, FrameGraphBlackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                auto& data{ blackboard.Get<BloomParameters>() };
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Emissive", data.GBufferEmissiveSampler, ResourceSlot::Slot_0 );

                for (UInt32 mipLevel{ 0 }; mipLevel < data.MipCount; ++mipLevel ) {
                    ctx.SetColorRenderTarget( "BloomDownSampling_ColorTarget", mipLevel );

                    PassRenderInfo info{ .ColorLoadOp{LoadOp::LOAD} };
                    ctx.BeginRender(info);

                    const auto dimensions{ InferDimensions( m_Resolution, mipLevel ) };
                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    ctx.BindPipeline( "BloomUpSampling_Pipeline" );
                    ctx.Draw( 3 );

                    ctx.EndRender();
                }
            } );

        // Final bloom composition pass
        graph.RegisterPass(
            "BloomBlend",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "BloomBlend_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, Multisampling::MSAA_X1, TextureUsage::COLOR );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::RGBA8_UNORM }
                };

                b.UseShader( "Resources/Shaders/slang/Bloom_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Bloom_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "BloomBlend_Pipeline", graphicsDesc );

                b.Read( "BloomDownSampling_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Write( "BloomBlend_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "FinalShadingPass_BufferEDGE+", FrameResourceState::RenderTarget );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "BloomDownSampling_ColorTarget", ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "FinalShadingPass_ColorTarget", ResourceSlot::Slot_1 );

                const auto dimensions{ InferDimensions( m_Resolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.SetColorRenderTarget( "BloomBlend_ColorTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "BloomBlend_Pipeline" );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
#endif
    }

    auto PostEffectsPass::RegisterTonemap( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct TonemapParameters {
            float Exposure{};
            float Gamma{};
            Int32 ToneMapType{};
        };

        graph.RegisterPass<TonemapParameters>(
            "Tonemap",
            [this]( FramePassBuilder &b, TonemapParameters& ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "Tonemap_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/Tonemap_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Tonemap_Frag.slang", ShaderStage::FRAGMENT );

                b.CreatePipeline( "Tonemap_Pipeline",
                    GraphicsPipelineDescription{
                    .VertexAttributesSpec{},
                    .ColorAttachmentFormats{ TextureFormat::RGBA8_UNORM }
                } );

                b.Write( "Tonemap_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "ChromaticAberration_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Read( "FinalShadingPass_BufferEDGE+", FrameResourceState::UniformBuffer );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                auto &pbrRadianceData{ blackboard.Get<FinalShadingConstants>() };
                auto &tonemappingData{ blackboard.Get<TonemapParameters>() };

                tonemappingData.Exposure = pbrRadianceData.Exposure;
                tonemappingData.Gamma = pbrRadianceData.Gamma;
                tonemappingData.ToneMapType = pbrRadianceData.ToneMap;

                ctx.PushConstants( MKT_ADDRESSOF( tonemappingData ), MKT_SIZEOF( tonemappingData ) );

                const auto dimensions{ InferDimensions( m_Resolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindImageSampler( ResourceGroup::DynamicSamplers, "FinalShadingPass_ColorTarget", ResourceSlot::Slot_0 );

                ctx.SetColorRenderTarget( "Tonemap_ColorTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "Tonemap_Pipeline" );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterPostProcessMaterialsFilter( FrameGraph& graph ) -> void {
        graph.RegisterPass(
           "PostProcessMaterials_Filter",
           []( FramePassBuilder &b ) {
               MKT_BEGIN_PROFILER_NAMED();
           },
           []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
               MKT_BEGIN_PROFILER_NAMED();
           } );
    }

    auto PostEffectsPass::RegisterGradient( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "ColorGradient",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "ColorGradient_ColorTarget", m_Resolution, TextureFormat::RGBA16_FLOAT, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/ColorGradient_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/ColorGradient_Frag.slang", ShaderStage::FRAGMENT );

                b.CreatePipeline( "ColorGradient_Pipeline",
                    GraphicsPipelineDescription{
                    .VertexAttributesSpec{},
                    .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                } );

                b.Read( "FinalShadingPass_BufferEDGE+", FrameResourceState::UniformBuffer );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                auto &pbrRadianceData{ blackboard.Get<FinalShadingConstants>() };

                m_ColorGradientParameters.Exposure = pbrRadianceData.Exposure;

                ctx.PushConstants( MKT_ADDRESSOF( m_ColorGradientParameters ), MKT_SIZEOF( m_ColorGradientParameters ) );

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "FinalShadingPass_ColorTarget", ResourceSlot::Slot_0 );

                ctx.SetColorRenderTarget( "ColorGradient_ColorTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "ColorGradient_Pipeline" );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::TraverseTextList( CommandContext& ctx ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TransformComponent, TextComponent>() };

        m_GlyphCount = 0;

        for ( auto& entity: renderables ) {
            auto& textComponent{ registry.get<TextComponent>( entity ) };

            if ( !textComponent.HasFont() ) {
                continue;
            }

            auto& transform{ registry.get<TransformComponent>( entity ) };
            SetupTextForRender( ctx, transform, textComponent );
        }
    }

    auto PostEffectsPass::SetupTextForRender( CommandContext& context, const TransformComponent& transformComponent, const TextComponent& textComponent ) -> void {
        using namespace StringUtils;

        MKT_BEGIN_PROFILER_NAMED();

        FontHandle font{ textComponent.GetFontHandle() };

        const float fontSize{ textComponent.GetSize() };
        const glm::vec4& color{ textComponent.GetColor() };
        const Camera* camera{ textComponent.GetCamera() };
        const glm::vec4& position{ transformComponent.GetTranslation(), 1.0f };

        double xPos{ position.x };
        double yPos{ position.y };
        double scale{ fontSize / font->GetSize() };

        double lineHeight{ font->GetMaxHeight() * scale };

        for ( const auto& character: textComponent.GetContents() ) {
            if ( IsLineFeed( character ) ) {
                xPos = position.x;
                yPos += lineHeight;
                continue;
            }

            double advance{ 0 };

            if ( font->HasGlyph( character ) ) {
                const FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( character ) ) };

                if ( !IsSpace( character ) ) {
                    // Quad Coordinates
                    double x0{ xPos + glyph.m_PlaneBounds.x * fontSize };
                    double y0{ yPos - glyph.m_PlaneBounds.y * fontSize };

                    // UV Coordinates
                    TextureHandle atlas{ font->GetAtlas() };
                    double s0{ glyph.m_AtlasBounds.x / atlas->GetWidth() };
                    double t0{ glyph.m_AtlasBounds.w / atlas->GetHeight() };
                    double s1{ glyph.m_AtlasBounds.z / atlas->GetWidth() };
                    double t1{ glyph.m_AtlasBounds.y / atlas->GetHeight() };

                    TextRenderParams fontParams{
                        .Model{ textComponent.IsWorldText() ? transformComponent.GetTransform() : Mat4F{ 1.0f } },
                        .Position{ x0, y0 + std::round( ( font->GetMaxHeight() * scale ) ) - ( glyph.m_Height * scale ), position.z, position.w },
                        .Size{ glyph.m_Width * scale, glyph.m_Height * scale, 0.0f, 0.0f },
                        .Color{ color },
                        .TexCoords{ { s0, t0 }, { s1, t0 }, { s1, t1 }, { s0, t1 } },
                        .TexIndex{ static_cast<UInt32>( context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", atlas ) ) }
                    };

                    Mat4F view{};
                    Mat4F projection{};

                    if ( !textComponent.IsWorldText() ) {
                        if ( camera ) {
                            view = camera->GetProjection();
                            projection = camera->GetViewMatrix();
                        } else {
                            const auto dimension{ InferDimensions( m_Resolution ) };
                            projection = glm::ortho( 0.0f, dimension.first, dimension.second, 0.0f, -1.0f, 1.0f );
                            view = glm::mat4{ 1.0f };
                        }
                    } else {
                        view = m_Camera->GetProjection();
                        projection = m_Camera->GetViewMatrix();
                    }
                    
                    fontParams.Proj = view;
                    fontParams.View = projection;
                    
                    m_TextInfo[m_GlyphCount++] = fontParams;
                }

                advance = glyph.m_AdvanceX * fontSize;
            } else {
                // If the character does not exist just insert space
                // equal to Space character.
                const FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( ' ' ) ) };
                advance = glyph.m_AdvanceX * fontSize;
            }

            xPos += advance;
        }
    }
}