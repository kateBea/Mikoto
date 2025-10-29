//
// Created by zanet on 3/15/2025.
//

#include <Logging/Logger.hh>

#include "Renderer/FontFactory.hh"

namespace Mikoto {

    FontFactory::FontFactory( const FontFactoryCreateInfo &options ) {}

    auto FontFactory::Init() -> void {
        m_FreeTypeHandle = msdfgen::initializeFreetype();

        m_IsInitialized = true;
    }

    auto FontFactory::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        msdfgen::deinitializeFreetype( m_FreeTypeHandle );

        m_IsInitialized = false;
    }

    auto FontFactory::LoadFont( const FontLoadDescription &description ) -> FontHandle {
        Font* font{ nullptr };

        return FontHandle::CreateEmpty();
    }
}// namespace Mikoto