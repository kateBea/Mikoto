//
// Created by kate on 10/13/25.
//

#include <Assets/AssetsService.hh>
#include <Filesystem/FileService.hh>
#include <GraphicsLayer.hh>
#include <Memory/Allocator.hh>
#include <Renderer/RenderUtility.hh>
#include <Renderer/RenderService.hh>

namespace Mikoto {

    GraphicsLayer::GraphicsLayer( std::string_view name )
        : ILayer{ name }
    {}

    auto GraphicsLayer::OnCreate() -> void {
        MKT_FILE_LOGGER_DEBUG( "Initializing Graphics Layer" );
        // Some example data: a few floats for a vertex buffer
        std::array vertexData{
            0.0f, 0.5f, 0.0f,  // Vertex 1 (x, y, z)
            -0.5f, -0.5f, 0.0f,// Vertex 2
            0.5f, -0.5f, 0.0f  // Vertex 3
        };

        // Describe the buffer
        BufferDescription desc{};
        desc.WithSizeBytes( vertexData.size() * sizeof( vertexData[0] ) )
                .WithData( AsBytes( vertexData.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_FLOAT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        // Create it through the GPU device (Vulkan or otherwise)
        const auto gpuDev{ RenderService::Get()->GetGpuDevice() };
        m_VertexBuffer = gpuDev->CreateBuffer( desc );

        // Allocate staging buffer to copy over the texture data
        BufferDescription stagingDesc{};
        stagingDesc.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_STAGING )
                .WithSizeBytes( MKT_MEGABYTES( 10 ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
        m_StagingBuffer = gpuDev->CreateBuffer( stagingDesc );

        TextureLoadDescription loadDesc{};
        loadDesc
                .WithFile( FileService::Get()->LoadFile( "./texture.png" ) )
                .WithType( TextureType::TEXTURE_2D );

        m_Texture = AssetsService::Get()->LoadAsset<Texture>( loadDesc );

        ModelLoadDescription modelLoadDesc{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/2 - Cat with scarf/source/Pbr/base.obj" ) },
            .WantTextures{ true }
        };

        m_Model = AssetsService::Get()->LoadAsset<Model>( modelLoadDesc );

        SetupScene();
    }

    auto GraphicsLayer::OnDestroy() -> void {
    }

    auto GraphicsLayer::OnUpdate( float ) -> void {
    }

    auto GraphicsLayer::SetupScene() -> void {

    }

}// namespace Mikoto
