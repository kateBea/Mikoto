//
// Created by zanet on 2/9/2025.
//

#include <Renderer/Vulkan/VulkanShaderLibrary.hh>

namespace Mikoto {
    auto VulkanShaderLibrary::Init() -> void {
    }
    auto VulkanShaderLibrary::Shutdown() -> void {
        s_Shaders.clear();
    }

    auto VulkanShaderLibrary::GetShader( const Path_T &filePath ) -> VulkanShader * {
        auto it{ s_Shaders.find( filePath.string() ) };

        if (it != s_Shaders.end()) {
            return it->second.get();
        }

        return nullptr;
    }

    auto VulkanShaderLibrary::LoadShader( const VulkanShaderCreateInfo &loadInfo ) -> VulkanShader * {
        auto result{ GetShader( loadInfo.FilePath )};

        if (result != nullptr) {
            return result;
        }

        auto [it, success] {
            s_Shaders.try_emplace( loadInfo.FilePath.string(), CreateScope<VulkanShader>( loadInfo ) )
        };

        if (success) {
            return it->second.get();
        }

        return nullptr;
    }
}