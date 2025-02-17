//
// Created by zanet on 1/27/2025.
//

#ifndef SHADERLIBRARY_HH
#define SHADERLIBRARY_HH

#include <unordered_map>

#include <Material/Core/Shader.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    class VulkanShaderLibrary {
    public:
        static auto GetShader( const Path_T &filePath ) -> VulkanShader *;
        static auto LoadShader( const VulkanShaderCreateInfo &loadInfo ) -> VulkanShader *;

    private:
        inline static std::unordered_map<std::string, Scope_T<VulkanShader>> s_Shaders{};
    };

}// namespace Mikoto


#endif // SHADERLIBRARY_HH
