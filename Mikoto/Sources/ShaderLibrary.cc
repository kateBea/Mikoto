//
// Created by zanet on 2/9/2025.
//

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderService.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <Material/ShaderLibrary.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {

    ShaderLibrary::ShaderLibrary( const ShaderLibraryDescription &options )
        : m_Device{ options.Device }, m_RootPath{ options.RootPath }
    {}

    auto ShaderLibrary::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_IsInitialized = true;
    }

    auto ShaderLibrary::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        m_Shaders.clear();

        m_IsInitialized = false;
    }

    auto ShaderLibrary::GetShader( const std::string_view uri ) -> ShaderModuleHandle {
        // conmvert path to absolute first
        auto it{ m_Shaders.find( uri.data() ) };

        if ( it != m_Shaders.end() ) {
            return m_Shaders.at( uri.data() );
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle {
        MKT_BEGIN_PROFILER_NAMED();

        // Get the file name and concat wioth root, create the whole path and store with it

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