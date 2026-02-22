/**
 * VulkanShader.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <filesystem>
#include <fstream>

// Third-Party Libraries
#include "volk.h"
#include <spirv_reflect.h>
#include <slang.h>
#include <slang-com-ptr.h>

// Project Headers
#include <Common/Common.hh>
#include <Common/String.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

#include <Core/Profiler.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    VulkanShader::VulkanShader( const ShaderModuleDescription& createInfo )
        : ShaderModule{
              createInfo.Stage,
              reinterpret_cast<const void*>( createInfo.ShaderFile->GetFileContents().data() ),
              createInfo.ShaderFile->GetSizeBytes()
          },
          m_Path{ createInfo.ShaderFile ? createInfo.ShaderFile->GetPath() : "" } {
        m_DebugName = createInfo.ShaderFile->GetName();
    }

    auto VulkanShader::GetNativeHandle( ObjectType object ) -> Object {
        return Object(m_Module );
    }

    auto VulkanShader::Initialize() -> void {
        if (!HasContents()) {
            MKT_CORE_LOGGER_ERROR( "VulkanShader::Initialize - Shader has no contents." );
            return;
        }

        // [DEBUG]. Test slang integration to slowly migrate to it
        if ( !m_Path.empty() ) {
            LoadSlang( m_Path );
        }

        // Check client specified correctly the stage
        SpvReflectShaderModule module{};
        SpvReflectResult result{ spvReflectCreateShaderModule(GetContentSize(), GetContents(), &module) };

        if (result == SPV_REFLECT_RESULT_SUCCESS) {
            // Number of entrypoints
            VkShaderStageFlagBits moduleStage{ static_cast<VkShaderStageFlagBits>( module.shader_stage ) };

            if (VulkanHelpers::ToVkStage( m_Stage ) != moduleStage) {
                MKT_CORE_LOGGER_WARN( "VulkanShader::Initialize - Specified wrong stage for shader {}. Changing to right type.", m_DebugName );
            }

            m_Stage = VulkanHelpers::FromVkStage( moduleStage );
        }

        VkShaderModuleCreateInfo moduleCreateInfo{ VulkanHelpers::Initializers::ShaderModuleCreateInfo() };
        moduleCreateInfo.codeSize = GetContentSize();
        moduleCreateInfo.pCode = static_cast<const UInt32*>(GetContents());

        if ( vkCreateShaderModule( VK_DEVICE(m_Device),
                                   std::addressof( moduleCreateInfo ),
                                   nullptr,
                                   std::addressof( m_Module ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanShader::Initialize - Failed to create shader module" );
        }

        m_StageCreateInfo = VulkanHelpers::Initializers::PipelineShaderStageCreateInfo();
        m_StageCreateInfo.stage = VulkanHelpers::ToVkStage( m_Stage );
        m_StageCreateInfo.module = m_Module;
        m_StageCreateInfo.pName = m_EntryPoint.c_str();
        m_StageCreateInfo.flags = 0;
        m_StageCreateInfo.pNext = nullptr;
        m_StageCreateInfo.pSpecializationInfo = nullptr;

        m_IsAllocated = true;
    }

    auto VulkanShader::LoadSlang( const Path& path ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        Path glslFile{ path };
        glslFile.replace_extension();
        
        const std::string moduelPath{ StringUtil::Format( "{}.glsl", glslFile.string() ) };
        const std::string moduelName{ path.filename().string() };

        auto globalSession{ ShaderLibrary::Get()->GetSlangGlobalSession() };
        auto slangTargets{ std::to_array<slang::TargetDesc>( { { .format{ SLANG_SPIRV },
                                                                 .profile{ globalSession->findProfile( "spirv_1_4" ) } } } ) };
        auto slangOptions{ std::to_array<slang::CompilerOptionEntry>( { { slang::CompilerOptionName::EmitSpirvDirectly,
                                                                          { slang::CompilerOptionValueKind::Int, 1 } } } ) };
        
        slang::SessionDesc slangSessionDesc{
            .targets{ slangTargets.data() },
            .targetCount{ SlangInt( slangTargets.size() ) },
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .compilerOptionEntries{ slangOptions.data() },
            .compilerOptionEntryCount{ ( UInt32 )slangOptions.size() }
        };

        Slang::ComPtr<slang::ISession> slangSession{};
        globalSession->createSession( slangSessionDesc, slangSession.writeRef() );

        Slang::ComPtr<slang::IModule> slangModule{ slangSession->loadModuleFromSource( "Test", "Resources/Shaders/slang/HelloTriangle_Frag.slang", nullptr, nullptr ) };

        Slang::ComPtr<ISlangBlob> spirv{};
        slangModule->getTargetCode( 0, spirv.writeRef() );
    }

    auto VulkanShader::Release() -> void {
        vkDestroyShaderModule( VK_DEVICE( m_Device ), m_Module, nullptr );

        m_IsAllocated = false;
    }

    auto VulkanShader::GetPipelineStageCreateInfo() const -> const VkPipelineShaderStageCreateInfo& {
        return m_StageCreateInfo;
    }

    auto VulkanShader::GetVulkanStage() const -> VkShaderStageFlags {
        return m_StageCreateInfo.stage;
    }

    VulkanShader::~VulkanShader() {
        if ( m_IsAllocated ) {
            Release();
        }
    }
}