//
// Created by zanet on 12/23/2025.
//

#include "Renderer/Core/SRGBase.hh"

namespace Mikoto {

    auto SRGPerPass::SetBuffer( std::string_view name, UInt32 binding, ShaderResourceType type ) -> void {
        Entry newEntry{
            .Name{ name },
            .Binding{ binding },
            .Type{ type }
        };

        m_Resources.emplace_back( std::move(newEntry) );
    }

    auto SRGTextures::GetMaxBindlessTextureCount() -> UInt32 {
        return 4096;
    }

    auto SRGTextures::Bind( TextureHandle texture, SamplerHandle sampler ) -> Int32 {
        if (m_Resources.contains( std::make_pair(texture.GetRaw(), sampler.GetRaw()) )) {
            return INVALID_TEXTURE_INDEX;
        }

        const auto [it, success] {
            m_Resources.try_emplace( std::make_pair(texture.GetRaw(), sampler.GetRaw()), m_Resources.size() )
        };

        if (success) {
            return it->second;
        }

        return INVALID_TEXTURE_INDEX;
    }
}// namespace Mikoto