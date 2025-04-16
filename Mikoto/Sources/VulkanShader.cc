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
#include <Core/Logger.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    VulkanShader::VulkanShader( const ShaderModuleDescription& createInfo )
        : ShaderModule{ createInfo.Stage, createInfo.ShaderContents }, m_EntryPoint{ "main" }
    {}

    auto VulkanShader::Allocate() -> void {
        if (!HasContents()) {
            MKT_CORE_LOGGER_ERROR( "VulkanShader::Allocate - Shader has no contents." );
            return;
        }

        VkShaderModuleCreateInfo moduleCreateInfo{ VulkanHelpers::Initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = GetContentSize();
        moduleCreateInfo.pCode = Reinterpret<const UInt32_T>( GetContents() );

        if ( vkCreateShaderModule( Dynamic<VulkanDevice>(m_Device)->GetLogicalDevice(),
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

        m_IsAllocated = true;
    }

    auto VulkanShader::Release() -> void {
        if (!m_IsAllocated) {
            return;
        }

        Dynamic<VulkanDevice>(m_Device)->FreeResource( this );
        vkDestroyShaderModule( Dynamic<VulkanDevice>(m_Device)->GetLogicalDevice(), m_Module, nullptr );
    }

    VulkanShader::~VulkanShader() {
        if ( !m_IsAllocated ) {
            Release();
        }
    }
}