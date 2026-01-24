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

#include <Math/Math.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Core/FrameGraphBlackboard.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/IBLPasses.hh>

namespace Mikoto {

    IBLPasses::IBLPasses( RenderResolution resolution )
        : m_Resolution{ resolution } {}

    auto IBLPasses::SetScene( Scene *scene ) -> void { m_Scene = scene; }

    auto IBLPasses::RegisterPasses( FrameGraph &graph ) -> void {
        RegisterSkybox( graph );
        RegisterPrefilter( graph );
        RegisterIrradiance( graph );
        RegisterBRDFLut( graph );
        RegisterShading( graph );
    }

    auto IBLPasses::SetResolution( RenderResolution resolution ) -> void {
        m_Resolution = resolution;

        // Update passes
    }

    auto IBLPasses::RegisterIrradiance( FrameGraph &graph ) -> void {
        struct IrradianceData {
            UInt32 MipLevels;
            UInt32 Dimensions;
            // Ubo data
        };

        graph.RegisterPass<IrradianceData>(
                "IrradiancePass",

                [&]( FramePassBuilder &b, IrradianceData &data ) {
                    // Create target
                    data.MipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( data.Dimensions ) ) ) + 1;
                    b.Create<TextureCube>( "IrradiancePass_ColorTarget", data.Dimensions, TextureFormat::RGBA32_FLOAT, data.MipLevels );

                    // Create pipeline
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "IrradiancePass_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                    b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget_Color );
                    b.Read( "SkyboxPass_TextureCube", FrameResourceState::ShaderResource_Read );

                    // b.BindBuffer( "IrradiancePass_CameraInfo", SRGType::SRG_PerPass, 0 );
                    // b.BindBuffer( "IrradiancePass_Parameters", SRGType::SRG_PerPass, 1 );
                    // b.BindTexture( "SkyboxPass_TextureCube", SRGType::SRG_PerPass, 2, "SkyboxPass_Sampler" );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard &blackboard ) {
                    const IrradianceData &data{ blackboard.Get<IrradianceData>() };

                    ctx.BindPipeline( "IrradiancePass_Pipeline" );
                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    for (UInt32 mip = 0; mip < data.MipLevels; ++mip) {
                        for (uint32_t face = 0; face < 6; ++face) {
                            ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                            // update UBOs
                            //ctx.FillBuffer( "IrradiancePass_CameraInfo", &ubo, sizeof( ubo ) );

                            ctx.BeginRender();
                            ctx.Draw( 6 );
                            ctx.EndRender();
                        }
                    }

                    ctx.EndPass();
                } );
    }

    auto IBLPasses::RegisterPrefilter( FrameGraph &graph ) -> void {
        struct PrefilterData {
            UInt32 MipLevels;
            UInt32 Dimensions;
            // Ubo data
        };

        graph.RegisterPass<PrefilterData>(
                "PrefilterPass",

                [&]( FramePassBuilder &b, PrefilterData &data ) {
                    // Create target
                    data.MipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( data.Dimensions ) ) ) + 1;
                    b.Create<TextureCube>( "IrradiancePass_ColorTarget", data.Dimensions, TextureFormat::RGBA32_FLOAT, data.MipLevels );

                    // Create pipeline
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "IrradiancePass_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                    b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget_Color );
                    b.Read( "SkyboxPass_TextureCube", FrameResourceState::ShaderResource_Read );

                    // b.BindBuffer( "IrradiancePass_CameraInfo", SRGType::SRG_PerPass, 0 );
                    // b.BindBuffer( "IrradiancePass_Parameters", SRGType::SRG_PerPass, 1 );
                    // b.BindTexture( "SkyboxPass_TextureCube", SRGType::SRG_PerPass, 2, "SkyboxPass_Sampler" );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard &blackboard ) {
                    const PrefilterData &data{ blackboard.Get<PrefilterData>() };

                    ctx.BindPipeline( "IrradiancePass_Pipeline" );
                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    for (UInt32 mip = 0; mip < data.MipLevels; ++mip) {
                        for (uint32_t face = 0; face < 6; ++face) {
                            ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                            // update UBOs
                            //ctx.FillBuffer( "IrradiancePass_CameraInfo", &ubo, sizeof( ubo ) );

                            ctx.BeginRender();
                            ctx.Draw( 6 );
                            ctx.EndRender();
                        }
                    }

                    ctx.EndPass();
                } );
    }

    auto IBLPasses::RegisterBRDFLut( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "BRDFLut",
                [this]( FramePassBuilder &b ) {
                    b.Create<Texture>( "BRDFLutPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "BRDFLutPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/BRDFLut_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/BRDFLut_Frag.sprv", ShaderStage::FRAGMENT );

                    b.Create<Pipeline>( "BRDFLutPass_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                    b.Write( "BRDFLutPass_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "BRDFLutPass_DepthTarget", FrameResourceState::ShaderResource_Read );
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    ctx.BindPipeline( "BRDFLutPass_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.SetColorRenderTarget( "BRDFLutPass_ColorTarget" );
                    ctx.SetDepthRenderTarget( "BRDFLutPass_DepthTarget" );

                    ctx.BeginRender();

                    ctx.Draw( 3 );

                    ctx.EndRender();
                } );
    }

    auto IBLPasses::EnableSkybox( bool enable ) -> void { m_UseSkybox = enable; }

    auto IBLPasses::SetCubeMap( TextureHandle cubeMap ) -> void { m_CubeMap = cubeMap; }

    auto IBLPasses::SetExposure( float value ) -> void { m_SkyboxUBO.Exposure = value; }

    auto IBLPasses::SetGamma( float value ) -> void { m_SkyboxUBO.Gamma = value; }

    auto IBLPasses::RegisterSkybox( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "Skybox",
                [this]( FramePassBuilder &b ) {
                    b.Create<Texture>( "FinalShadingPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "FinalShadingPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.Create<Buffer>( "SkyboxPass_CameraInfo", BufferUsage::UNIFORM, sizeof( SkyboxUBO ), 1 );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/Skybox_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Skybox_Frag.sprv", ShaderStage::FRAGMENT );

                    GraphicsPipelineDescription graphicsDesc{};
                    graphicsDesc.DepthTest = false;
                    graphicsDesc.DepthWrite = false;
                    graphicsDesc.AlphaBlending = false;
                    graphicsDesc.PipelineCullMode = CullMode::NONE;
                    graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;
                    b.Create<Pipeline>( "SkyboxPass_Pipeline", graphicsDesc );

                    b.Write( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "FinalShadingPass_DepthTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "SkyboxPass_CameraInfo", FrameResourceState::Transfer_Dst );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    if (!m_UseSkybox) { return; }

                    ctx.BindPipeline( "SkyboxPass_CameraInfo" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.UploadBuffer<SkyboxUBO>( "SkyboxPass_CameraInfo", m_SkyboxUBO );

                    ctx.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );
                    ctx.SetColorRenderTarget( "FinalShadingPass_ColorTarget" );
                    ctx.SetDepthRenderTarget( "FinalShadingPass_DepthTarget" );

                    ctx.BeginRender();

                    ctx.Draw( 36 );

                    ctx.EndRender();
                } );
    }

    auto IBLPasses::SetCamera( const Camera *camera ) -> void {
        m_FrameUBO.View = camera->GetViewMatrix();
        m_FrameUBO.Projection = camera->GetProjection();
        m_FrameUBO.CameraPosition = Vec4F{ camera->GetPosition(), 1.0f };
    }

    auto IBLPasses::RegisterShading( FrameGraph &graph ) -> void {
        struct FinalShading {
            Scene *ActiveScene{};
        };

        graph.RegisterPass<FinalShading>(
                "FinalShading",
                []( FramePassBuilder &b, FinalShading &data ) -> void {
                    b.Create<Buffer>( "HelloTexture_TexturesBuffer", BufferUsage::UNIFORM, sizeof( FinalShading ) );
                    b.Create<Texture>( "HelloTexture_ColorTarget", RenderResolution::FHD_1080, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "HelloTexture_DepthTarget", RenderResolution::FHD_1080, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/FullscreenTriangle_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/FullscreenTriangle_Frag.sprv", ShaderStage::FRAGMENT );

                    b.Create<Pipeline>( "HelloTexture_Pipeline", GraphicsPipelineDescription{
                                            .PrimitiveTopology{ Topology::TRIANGLE_STRIP },
                                            .VertexAttributesSpec{}, } );

                    b.Write( "HelloTexture_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "HelloTexture_DepthTarget", FrameResourceState::ShaderResource_Read );

                    b.Write( "HelloTexture_TexturesBuffer", FrameResourceState::ShaderResource_Read );
                    b.UseSrg( SRGType::SRG_PerPass, "HelloTexture_TexturesBuffer", 0 );
                    b.UseSrg( SRGType::SRG_Textures );
                },
                []( CommandContext &ctx, FrameGraphBlackboard &blackboard ) -> void {
                    auto &data{ blackboard.Get<FinalShading>() };

                    ctx.UploadBuffer<FinalShading>( "HelloTexture_TexturesBuffer", std::addressof( data ) );

                    ctx.BindPipeline( "HelloTexture_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.SetClearColor( { 0.5f, 0.2f, 0.3f, 1.0f } );
                    ctx.SetColorRenderTarget( "HelloTexture_ColorTarget" );
                    ctx.SetDepthRenderTarget( "HelloTexture_DepthTarget" );
                    ctx.BeginRender();

                    ctx.Draw( 4, 1, 0, 0 );

                    ctx.EndRender();
                } );
    }

    auto IBLPasses::UploadInstanceData( CommandContext &context ) -> void {
        Size meshIndex{};
        Size firstInstance{};

        for (auto &[meshNode, instanceInfo]: m_MeshDrawState) {
            DrawIndexedState &drawState{ instanceInfo.InstanceDrawState };

            drawState.IndexBuffer = meshNode->GetIndexBuffer();

            if (drawState.VertexBuffers.empty()) { drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0 ); }

            Size drawCount{};
            for (const auto &[entityID, meshInstanceInfo]: instanceInfo.InstanceInfos) {
                if (instanceInfo.IsActive( entityID )) {
                    m_Meshes[meshIndex++] = meshInstanceInfo;
                    ++drawCount;


                    // We need to ensure it does not get drawn later
                    instanceInfo.Disable( entityID );
                }
            }

            drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawState.FirstInstance = firstInstance;
            drawState.InstancesCount = drawCount;

            firstInstance += drawCount;

            context.DrawIndexed( drawState );
        }

        // Upload data
        context.UploadBufferData( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMaterialParams ), firstInstance );
    }

    auto IBLPasses::TraverseMeshList( CommandContext &context ) -> void {
        auto &registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for (auto &entity: renderables) {
            auto &tag{ registry.get<TagComponent>( entity ) };
            auto &transform{ registry.get<TransformComponent>( entity ) };
            auto &meshComponent{ registry.get<MeshComponent>( entity ) };
            auto &materialComp{ registry.get<MaterialComponent>( entity ) };

            MaterialHandle material{ materialComp.GetMaterial() };

            if (meshComponent.HasMesh() && !material.IsEmpty()) {
                MeshNode *meshNode{ meshComponent.GetMesh() };
                PBRMaterial *pbrMat{ dynamic_cast<PBRMaterial *>( material.GetRaw() ) };

                auto &[DrawIndexedState, ActiveEntities, Instances]{
                    m_MeshDrawState[meshNode]
                };

                ActiveEntities[tag.GetGUID()] = tag.IsActive();

                if (tag.IsActive()) {
                    ShaderMaterialParams &ubo{ Instances[tag.GetGUID()] };

                    ubo.Transform = transform.GetTransform();

                    ubo.Albedo = pbrMat->GetColor();
                    ubo.Factors.x = pbrMat->GetMetallicFactor();
                    ubo.Factors.y = pbrMat->GetRoughnessFactor();

                    ubo.AlbedoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                    ubo.NormalIndex = context.PushTexture( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
                    ubo.MetallicIndex = context.PushTexture( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
                    ubo.RoughnessIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                    ubo.AoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                }
            }
        }
    }
}// namespace Mikoto
