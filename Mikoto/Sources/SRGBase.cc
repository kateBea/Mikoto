//
// Created by zanet on 12/23/2025.
//

#include "Renderer/Core/SRGBase.hh"

namespace Mikoto {

    auto SRGPerPass::SetBuffer( std::string_view name, UInt32 binding, ShaderResourceType type ) -> void {
        if (m_RegisteredBuffers.contains( std::string{ name } )) {
            return;
        }

        Entry newEntry{
            .Name{ name },
            .Binding{ binding },
            .Type{ type }
        };

        m_Resources.emplace_back( std::move(newEntry) );
        m_RegisteredBuffers.emplace( std::string{ name });
    }

    auto SRGPerPass::SetTexture( std::string_view textureName, std::string_view samplerName, UInt32 binding ) -> void {
        const auto it{ m_RegisteredTextures.find( std::string{ textureName } ) };
        if ( it != m_RegisteredTextures.end() && it->second == samplerName ) {
            return;
        }

        Entry newEntry{
            .Name{ textureName },
            .SamplerName{ samplerName },
            .Binding{ binding },
            .Type{ ShaderResourceType::SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER }
        };

        m_Resources.emplace_back( std::move(newEntry) );
        m_RegisteredTextures.emplace( std::string{ textureName }, std::string{ samplerName } );
    }

    auto SRGTextures::GetMaxTextureCount() -> UInt32 {
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

    auto SRGTextures::Contains( TextureHandle texture, SamplerHandle sampler ) -> bool {
        return GetIndex( texture, sampler ) != INVALID_TEXTURE_INDEX;
    }

    auto SRGTextures::GetIndex( TextureHandle texture, SamplerHandle sampler ) -> Int32 {
        const auto it{ m_Resources.find( std::make_pair(texture.GetRaw(), sampler.GetRaw() ) ) };
        if (it != m_Resources.end()) {
            return it->second;
        }

        return INVALID_TEXTURE_INDEX;
    }
}// namespace Mikoto