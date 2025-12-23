//
// Created by zanet on 12/23/2025.
//

#include "Renderer/Core/ShaderResourceGroup.hh"

namespace Mikoto {

    auto ShaderResourceGroup::SetStorageBuffer( std::string_view name, UInt32 binding ) -> void {
        Entry newEntry{
            .Name{ name },
            .Binding{ binding },
            .Type{ ShaderResourceType::SHADER_STORAGE_BUFFER }
        };

        m_Resources.emplace_back( std::move(newEntry) );
    }
}