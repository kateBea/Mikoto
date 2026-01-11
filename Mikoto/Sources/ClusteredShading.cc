//
// Created by kate on 1/11/26.
//

#include <Material/ShaderLibrary.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

#include <Material/TextureCube.hh>

#include <Renderer/Passes/ClusteredShading.hh>

namespace Mikoto {
    auto AABBGenComp::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};
        pipelineDesc.Description = ComputePipelineDescription{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/AABBGen_Comp.sprv", ShaderStage::COMPUTE_STAGE );

        builder.CreateNamedPipeline( "AABBGenComp_Pipeline", pipelineDesc );

        BufferDescription cameraUBO{};
        cameraUBO.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof( CameraUBO ), 1 );
        builder.CreateNamedBuffer( "AABBGenComp_CameraUBO", cameraUBO );

        BufferDescription aabbBuffer{};
        aabbBuffer.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( m_NumClusters * sizeof( Cluster ) );
        builder.CreateNamedBuffer( "AABBGenComp_Clusters", aabbBuffer );

        builder.WriteBuffer( this, "AABBGenComp_Clusters" );
        builder.WriteBuffer( this, "AABBGenComp_CameraUBO" );
    }

    auto AABBGenComp::Execute( PassCommandList &commandList ) -> void {
        commandList.BeginCompute(this);
        commandList.BindPipeline( "AABBGenComp_Pipeline" );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        m_CameraUBO = {
            .ViewMatrix{ m_Camera->GetViewMatrix() },
            .InverseProjection{ glm::inverse(m_Camera->GetProjection()) },

            .GridSize{ glm::vec4{ m_GridSizeX, m_GridSizeY, m_GridSizeZ, 0.0f } },
            .ViewPosition{ glm::vec4{ m_Camera->GetPosition(), 0.0f } },

            .Screen{ m_Camera->GetNearPlane(), m_Camera->GetFarPlane(), 1920.0f, 1080.0f },
            .LightInfo{ m_CameraUBO.LightInfo.x }
        };

        commandList.FillBuffer( "AABBGenComp_CameraUBO", std::addressof( m_CameraUBO ), sizeof( CameraUBO ));

        commandList.Dispatch(m_GridSizeX, m_GridSizeY, m_GridSizeZ);

        commandList.EndCompute();
    }

    auto AABBGenComp::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto AABBGenComp::SetHeatMap( bool enable ) -> void {
        m_CameraUBO.LightInfo.x = enable == 1 ? 1.0 : 0.0;
    }

    auto LightCullingComp::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};
        pipelineDesc.Description = ComputePipelineDescription{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/LightCulling_Comp.sprv", ShaderStage::COMPUTE_STAGE );

        builder.CreateNamedPipeline( "LightCullingComp_Pipeline", pipelineDesc );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( sizeof(ShaderLightTypeParams) * m_Lights.size()  );
        builder.CreateNamedBuffer( "LightCullingComp_LightsBuffer", lightsBuffer );

        BufferDescription lightCulling{};
        lightCulling.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement(  sizeof(LightCullingUBO), 1 );
        builder.CreateNamedBuffer( "LightCullingComp_LightsCullingInfo", lightCulling );

        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );

        builder.WriteBuffer( this, "LightCullingComp_LightsBuffer" );
        builder.WriteBuffer( this, "LightCullingComp_LightsCullingInfo" );
    }

    auto LightCullingComp::Execute( PassCommandList &commandList ) -> void {
        MKT_ASSERT( m_NumClusters != 0, "Number of cluster must different to 0" );

        commandList.BeginCompute(this);
        commandList.BindPipeline( "LightCullingComp_Pipeline" );

        TraverseLights( commandList );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        const auto numWorkGroupsX{ (m_NumClusters + m_LocalSize - 1) / m_LocalSize };
        commandList.Dispatch(numWorkGroupsX, 1, 1);

        commandList.EndCompute();
    }

    auto LightCullingComp::SetClusterCount( UInt32 clusterCount ) -> void {
        m_NumClusters = clusterCount;
    }

    auto LightCullingComp::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto LightCullingComp::TraverseLights( const PassCommandList &commandList ) -> void {
        auto& registry{ m_Scene->GetRegistry() };
        auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        Int32 lightsCount{};

        for ( auto& lightEntity: lightsView ) {
            TagComponent& tag{ registry.get<TagComponent>( lightEntity ) };
            LightComponent& lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent& transformCom{ registry.get<TransformComponent>( lightEntity ) };

            if ( lightsCount >= MAX_LIGHTS ) {
                break;
            }

            auto& uboLight{ m_Lights[lightsCount] };

            if (!tag.IsActive()) {
                uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_INACTIVE );
                continue;
            }

            switch ( lightComp.GetActiveType() ) {
                case LightType::POINT_LIGHT_TYPE: {

                    auto& point{ lightComp.Get<PointLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Diffuse = Vec4F( point.GetColor(), 0.0f );

                    uboLight.Intensity = point.GetIntensity();
                    uboLight.Radius = point.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>(ShaderActiveLightType::LIGHT_TYPE_POINT);

                    break;
                }

                case LightType::SPOT_LIGHT_TYPE: {
                    auto& spot{ lightComp.Get<SpotLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Direction = Vec4F( spot.GetDirection(), 0.0f );
                    uboLight.Diffuse = Vec4F( spot.GetColor() * spot.GetIntensity(), 1.0f );

                    uboLight.CutOff = spot.GetCutOff();
                    uboLight.OuterCutOff = spot.GetOuterCutOff();

                    uboLight.Intensity = spot.GetIntensity();
                    uboLight.Radius = spot.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>(ShaderActiveLightType::LIGHT_TYPE_SPOT);

                    break;
                }

                case LightType::DIRECTIONAL_LIGHT_TYPE: {
                    auto& dir{ lightComp.Get<DirectionalLight>() };

                    uboLight.Direction = Vec4F( dir.GetDirection(), 0.0f );
                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );// optional for shadows
                    uboLight.Diffuse = Vec4F( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.ActiveLightType = static_cast<Int32>(ShaderActiveLightType::LIGHT_TYPE_DIRECTIONAL);

                    break;
                }
            }

            ++lightsCount;
        }

        m_LightCullingUBO.LightCount = lightsCount;

        // Copy to GPU buffer
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 2 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsCullingInfo", 3 );

        // Just copy the amount of active lights we visited
        commandList.FillBuffer( "LightCullingComp_LightsBuffer", m_Lights.data(), lightsCount  * sizeof(ShaderLightTypeParams));
        commandList.FillBuffer( "LightCullingComp_LightsCullingInfo", std::addressof( m_LightCullingUBO ), sizeof(LightCullingUBO) );
    }

    auto ShadowPass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};

        pipelineDesc.Description = graphicsDesc;

        builder.CreateNamedPipeline( "ShadowPass_Pipeline", pipelineDesc );

        builder.CreateColorRenderTarget( "ShadowPass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        builder.CreateDepthRenderTarget( "ShadowPass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        // Transform, texture indices, etc
        BufferDescription objectsInfo{};
        objectsInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( 0 );// TODO
        builder.CreateNamedBuffer( "ShadowPass_ObjectInfo", objectsInfo );

        // Camera
        BufferDescription camera{};
        objectsInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( 0 );// TODO
        builder.CreateNamedBuffer( "ShadowPass_CameraInfo", camera );

        // Declare its inputs and outputs
        builder.WriteTexture( this, "ShadowPass_ColorTarget" );
        builder.WriteTexture( this, "ShadowPass_DepthTarget" );
        builder.WriteBuffer( this, "ShadowPass_ObjectInfo" );
        builder.WriteBuffer( this, "ShadowPass_CameraInfo" );
    }

    auto ShadowPass::Execute( PassCommandList& commandList ) -> void {

        commandList.SetColorRenderTarget( "ShadowPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "ShadowPass_LightsBuffer" );

        commandList.BeginRender(this);

        // commandList.BindStorageBuffer( "ShadowPass_CameraInfo", 0, 0);
        // commandList.BindStorageBuffer( "ShadowPass_LightsBuffer", 1, 0);
        // commandList.BindStorageBuffer( "ShadowPass_ObjectInfo", 2, 0);

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.BindPipeline( "ShadowPass_Pipeline" );

        // Meshes
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComponent{ registry.get<MeshComponent>( entity ) };
            auto& materialComp{ registry.get<MaterialComponent>( entity ) };

            MaterialHandle material{ materialComp.GetMaterial() };

            if ( tag.IsActive() && meshComponent.HasMesh() && !material.IsEmpty() ) {
                PBRMaterial* matPtr{ dynamic_cast<PBRMaterial*>( material.GetRaw() ) };

                MeshNode* mesh{ meshComponent.GetMesh() };


            }
        }

        // Lights
        auto lights{ registry.view<TagComponent, TransformComponent, LightComponent>() };
        for ( auto& entity: lights ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& lightComp{ registry.get<LightComponent>( entity ) };
        }

        commandList.EndRender();
    }

    auto ShadowPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }
}
