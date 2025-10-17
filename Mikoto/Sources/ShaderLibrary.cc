//
// Created by zanet on 2/9/2025.
//

#include <Material/ShaderLibrary.hh>
#include <Filesystem/FileService.hh>
#include <Renderer/GpuDevice.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {

    ShaderLibrary::ShaderLibrary( const ShaderLibraryDescription &options )
        : m_Device{ options.Device }
    {}

    auto ShaderLibrary::Init() -> void {
        m_IsInitialized = true;
    }

    auto ShaderLibrary::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        m_Shaders.clear();

        m_IsInitialized = false;
    }

    auto ShaderLibrary::GetShader( const std::string_view uri ) -> ShaderModuleHandle {
        auto it{ m_Shaders.find( uri.data() ) };

        if ( it != m_Shaders.end() ) {
            return m_Shaders.at( uri.data() );
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle {
        auto it{ m_Shaders.find( path.string() ) };
        if ( it != m_Shaders.end() ) {
            return it->second;
        }

        ShaderModuleHandle shaderModuleHandle{ m_Device->LoadShader( path, stage ) };
        if (!shaderModuleHandle.IsEmpty()) {
            m_Shaders.try_emplace( path.string(), shaderModuleHandle );
            return m_Shaders.at( path.string() );
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::LoadShader( const ShaderModuleDescription &description ) -> ShaderModuleHandle {
        return this->LoadShader( description.ShaderFile->GetPath(), description.Stage );
    }
}