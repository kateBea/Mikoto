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

    auto FrameBlackboard::RegisterTexture( std::string_view name, TextureDescription description ) -> void {
        if (m_TexturesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameBlackboard::RegisterPipeline - Named render target [{}] already exists.", name );
            return;
        }

        TextureHandle texture{ m_Device->CreateTexture( description ) };

        if (!texture.IsEmpty()) {
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
            desc.DepthTexture = GetTexture( description.DepthRenderTargets );
            for (const auto &renderTargetName: description.ColorRenderTargets) { desc.ColorAttachments.emplace_back( GetTexture( renderTargetName ) ); }

            pipeline = m_Device->CreatePipeline( desc );
        } else if (std::holds_alternative<ComputePipelineDescription>( description.Description )) {
            // Compute pipeline ====================================
            ComputePipelineDescription &desc{ std::get<ComputePipelineDescription>( description.Description ) };

            for (auto &[stage, shaderPath]: description.Shaders) { desc.Stage = ShaderLibrary::Get()->LoadShader( shaderPath, stage ); }

            pipeline = m_Device->CreatePipeline( desc );
        }

        if (!pipeline.IsEmpty()) { m_PipelinesByNames.emplace( std::string{ name }, pipeline ); }
    }

    auto FrameBlackboard::RegisterBuffer( std::string_view name, BufferDescription description ) -> void {

    }
}
