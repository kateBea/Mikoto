//
// Created by kate on 12/19/25.
//

#include <Renderer/Core/FrameBlackboard.hh>

#include <Material/ShaderLibrary.hh>

namespace Mikoto {
    FrameBlackboard::FrameBlackboard( GpuDevice *device )
        : m_Device{ device } {
        MKT_ASSERT( m_Device, "Tried to create a frame blackboard with a NULL devide" );
    }

    auto FrameBlackboard::RegisterTexture( std::string_view name, TextureHandle handle ) -> void {
        m_TexturesByNames[std::string{ name }] = handle;
    }

    auto FrameBlackboard::GetTexture( std::string_view name ) -> TextureHandle {
        const auto it{ m_TexturesByNames.find( std::string{ name } ) };
        if (it != m_TexturesByNames.end()) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    auto FrameBlackboard::GetPipeline( std::string_view name ) -> PipelineHandle {
        const auto it{ m_PipelinesByNames.find( std::string{ name } ) };
        if (it != m_PipelinesByNames.end()) {
            return it->second;
        }

        return PipelineHandle::CreateEmpty();
    }

    auto FrameBlackboard::GetBuffer( std::string_view name ) -> BufferHandle {
        const auto it{ m_BuffersByNames.find( std::string{ name } ) };
        if (it != m_BuffersByNames.end()) {
            return it->second;
        }

        return BufferHandle::CreateEmpty();
    }

    auto FrameBlackboard::GetSampler( std::string_view name ) -> SamplerHandle {
        const auto it{ m_SamplersByNames.find( std::string{ name } ) };
        if (it != m_SamplersByNames.end()) {
            return it->second;
        }

        return SamplerHandle::CreateEmpty();
    }

    auto FrameBlackboard::RegisterTexture( std::string_view name, TextureDescription description ) -> void {
        if (m_TexturesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameBlackboard::RegisterTexture - Named texture [{}] already exists.", name );
            return;
        }

        TextureHandle texture{ m_Device->CreateTexture( description ) };

        if (!texture.IsEmpty()) {
            texture->SetDebugName( name );
            m_TexturesByNames.emplace( std::string{ name }, texture );
        }
    }

    auto FrameBlackboard::RegisterTexture( std::string_view name, TextureCubeCreateDescription description ) -> void {
        if (m_TexturesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameBlackboard::RegisterTexture - Named texture cube [{}] already exists.", name );
            return;
        }

        TextureHandle texture{ m_Device->CreateTexture( description ) };

        if (!texture.IsEmpty()) {
            texture->SetDebugName( name );
            m_TexturesByNames.emplace( std::string{ name }, texture );
        }
    }

    auto FrameBlackboard::RegisterPipeline( std::string_view name, PipelineDescription description ) -> void {
        if (m_PipelinesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameBlackboard::RegisterPipeline - Named pipeline [{}] already exists.", name );
            return;
        }

        PipelineHandle pipeline{ PipelineHandle::CreateEmpty() };

        if (std::holds_alternative<GraphicsPipelineDescription>( description.Description )) {
            // Graphics pipeline ====================================
            GraphicsPipelineDescription &desc{ std::get<GraphicsPipelineDescription>( description.Description ) };

            for (auto &[stage, shaderPath]: description.Shaders) { desc.ShaderStages.emplace_back( ShaderLibrary::Get()->LoadShader( shaderPath, stage ) ); }

            // TODO: find render target formats. We need to remove the need of passing the textures right now we do it because vulkan dynamic rendering arch needs it

            pipeline = m_Device->CreatePipeline( desc );
        } else if (std::holds_alternative<ComputePipelineDescription>( description.Description )) {
            // Compute pipeline ====================================
            ComputePipelineDescription &desc{ std::get<ComputePipelineDescription>( description.Description ) };

            for (auto &[stage, shaderPath]: description.Shaders) { desc.Stage = ShaderLibrary::Get()->LoadShader( shaderPath, stage ); }

            pipeline = m_Device->CreatePipeline( desc );
        }

        if (!pipeline.IsEmpty()) {
            pipeline->SetDebugName( name );
            m_PipelinesByNames.emplace( std::string{ name }, pipeline );
        }
    }

    auto FrameBlackboard::RegisterBuffer( std::string_view name, BufferDescription description ) -> void {
        if (m_BuffersByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameBlackboard::RegisterBuffer - Named buffer [{}] already exists.", name );
            return;
        }

        BufferHandle buffer{ m_Device->CreateBuffer( description ) };

        if (!buffer.IsEmpty()) {
            buffer->SetDebugName( name );
            m_BuffersByNames.emplace( std::string{ name }, buffer );
        }
    }

    auto FrameBlackboard::RegisterSample( std::string_view name, SamplerDescription description ) -> void {
        if (m_SamplersByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameBlackboard::RegisterSampler - Named sampler [{}] already exists.", name );
            return;
        }

        SamplerHandle buffer{ m_Device->CreateSampler( description ) };

        if (!buffer.IsEmpty()) {
            buffer->SetDebugName( name );
            m_SamplersByNames.emplace( std::string{ name }, buffer );
        }
    }
}
