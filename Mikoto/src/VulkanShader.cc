/**
 * VulkanShader.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <filesystem>
#include <fstream>

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include <Common/Common.hh>
#include <Core/Engine.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/FileSystem.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    VulkanShader::VulkanShader( const VulkanShaderCreateInfo& createInfo )
        : Shader{ createInfo.Stage }, m_EntryPoint{ "main" } {
        Upload( createInfo );
    }

    auto VulkanShader::Upload( const VulkanShaderCreateInfo& createInfo ) -> void {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
        const File* shaderFile{ fileSystem.LoadFile( createInfo.FilePath ) };

        if (shaderFile == nullptr) {
            MKT_CORE_LOGGER_ERROR( "VulkanShader::Upload - Failed to load shader file." );
            return;
        }

        m_File = shaderFile;

        VkShaderModuleCreateInfo moduleCreateInfo{ VulkanHelpers::Initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = m_File->GetFileContents().size();
        moduleCreateInfo.pCode = reinterpret_cast<const UInt32_T*>( m_File->GetFileContents().data() );

        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if ( vkCreateShaderModule( device.GetLogicalDevice(),
                                   std::addressof( moduleCreateInfo ),
                                   nullptr,
                                   std::addressof( m_Module ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanShader::CreateModule - Failed to create shader module" );
        }

        m_StageCreateInfo = VulkanHelpers::Initializers::PipelineShaderStageCreateInfo();
        m_StageCreateInfo.stage = VulkanHelpers::GetVkStageFromShaderStage( m_Stage );
        m_StageCreateInfo.module = m_Module;
        m_StageCreateInfo.pName = m_EntryPoint.c_str();
        m_StageCreateInfo.flags = 0;
        m_StageCreateInfo.pNext = nullptr;
        m_StageCreateInfo.pSpecializationInfo = nullptr;
    }

    auto VulkanShader::Release() -> void {
        vkDestroyShaderModule( VulkanContext::Get().GetDevice().GetLogicalDevice(), m_Module, nullptr );
    }

    VulkanShader::~VulkanShader() {
        if ( !m_IsReleased ) {
            Release();
            Invalidate();
        }
    }
}