//
// Created by zanet on 3/15/2025.
//

#ifndef FREETYPEMANAGER_HH
#define FREETYPEMANAGER_HH

#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Common/Singleton.hh>

#include <Assets/Font.hh>

namespace Mikoto {

    struct FontFactoryCreateInfo {

    };

    class FontFactory final : public IService, public Singleton<FontFactory> {
    public:
        explicit FontFactory( const FontFactoryCreateInfo &options );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto LoadFont(const FontLoadDescription& description) -> FontHandle;

    private:
        msdfgen::FreetypeHandle *m_FreeTypeHandle{ nullptr };
    };
}



#endif //FREETYPEMANAGER_HH
