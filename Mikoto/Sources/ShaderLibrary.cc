//
// Created by zanet on 2/9/2025.
//

#include <Assets/ShaderLibrary.hh>
#include <FileSystem/FileService.hh>
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
    }

    auto ShaderLibrary::GetShader( const std::string_view uri ) -> ShaderModuleHandle {
        auto it{ m_Shaders.find( uri.data() ) };

        if ( it != m_Shaders.end() ) {
            return m_Shaders.at( uri.data() );
        }

        return {};
    }

    auto ShaderLibrary::LoadShader( const ShaderModuleDescription &description ) -> ShaderModuleHandle {
        if (description.ShaderContents.empty() ) {
            return {};
        }

        ShaderModuleHandle moduleHandle{ m_Device->CreateShaderModule( description ) };
        auto [it, success] {
            m_Shaders.try_emplace( description.Uri, moduleHandle )
        };

        if (success) {
            return GetShader( description.Uri );
        }

        return {};
    }
}