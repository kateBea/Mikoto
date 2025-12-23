//
// Created by zanet on 12/23/2025.
//

#ifndef MIKOTO_SHADERRESOURCEGROUP_HH
#define MIKOTO_SHADERRESOURCEGROUP_HH

#include <vector>
#include <string_view>

#include <Library/Utility/Types.hh>

#include "FrameResource.hh"

namespace  Mikoto {

    enum class SRGType {
        SRG_Textures,
        SRG_PerFrame,
        SRG_PerCompute
    };

    class ShaderResourceGroup {
    public:

        auto SetStorageBuffer(std::string_view name, UInt32 binding) -> void;

        auto begin() -> decltype(auto) { return m_Resources.begin(); }
        auto end() -> decltype(auto) { return m_Resources.end(); }

        auto cbegin() const -> decltype(auto) { return m_Resources.cbegin(); }
        auto cend() const -> decltype(auto) { return m_Resources.cend(); }

    private:
        struct Entry {
            std::string Name{};
            UInt32 Binding{};
            ShaderResourceType Type{ ShaderResourceType::SHADER_RESOURCE_UNDEFINED };
        };

        std::vector<Entry> m_Resources{};
    };
}


#endif //MIKOTO_SHADERRESOURCEGROUP_HH