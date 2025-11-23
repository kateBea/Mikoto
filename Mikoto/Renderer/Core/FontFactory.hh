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
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {

    struct FontFactoryCreateInfo {
        GpuDevice *Device{ nullptr };
    };

    class FontFactory final : public IService, public Singleton<FontFactory> {
    public:
        explicit FontFactory( const FontFactoryCreateInfo &options );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto LoadFont(const FontLoadDescription& description) -> FontHandle;

    private:
        // =================================================================
        using MsdfAtlasGen = msdf_atlas::ImmediateAtlasGenerator<
                // pixel type of buffer for individual glyphs depends on generator function
                float,

                // number of atlas color channels
                4,

                // function to generate bitmaps for individual glyphs
                msdf_atlas::mtsdfGenerator,

                // Class that stores the atlas bitmap. For example, a custom atlas storage class that stores it in VRAM can be used.
                msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;

        using MsdfGlyphGeometryList = std::vector<msdf_atlas::GlyphGeometry>;

        // =================================================================
        struct MsdfData {
            Int32 AtlasWidth{};
            Int32 AtlasHeight{};
            std::vector<Byte> Bytes{};

            MsdfGlyphGeometryList GlyphData{};
        };

        struct CharsetRange {
            Int32 Start{};
            Int32 End{};
        };

    private:
        auto GenerateAtlas( const CStr fontFilename ) const -> MsdfData;

    private:
        GpuDevice *m_GpuDevice{ nullptr };
        msdfgen::FreetypeHandle *m_FreeTypeHandle{ nullptr };
    };
}



#endif //FREETYPEMANAGER_HH
