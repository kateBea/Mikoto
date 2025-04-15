//
// Created by zanet on 3/15/2025.
//

#include <Core/Logger.hh>

#include "Renderer/FontService.hh"

namespace Mikoto {

    FontService::FontService( const FontServiceCreateInfo &options ) {}

    auto FontService::Init() -> void {
        m_FreeTypeHandle = msdfgen::initializeFreetype();

        m_IsInitialized = true;
    }

    auto FontService::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        msdfgen::deinitializeFreetype( m_FreeTypeHandle );

        m_IsInitialized = false;
    }

    auto FontService::LoadFont( const FontLoadDescription &description ) -> Font * {

    }
}// namespace Mikoto