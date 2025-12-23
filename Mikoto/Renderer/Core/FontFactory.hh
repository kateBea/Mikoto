//
// Created by zanet on 3/15/2025.
//

#ifndef FREETYPEMANAGER_HH
#define FREETYPEMANAGER_HH

// I love Windows.h defining min and max macros that break everything
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <vector>

#include <ankerl/unordered_dense.h>
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

        auto LoadFont(const FontLoadDescription& description ) -> FontHandle;

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

        using MTSDFGen = msdf_atlas::ImmediateAtlasGenerator<float, 4, &msdf_atlas::mtsdfGenerator,
            msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;

        // =================================================================

        struct CharsetRange {
            Int32 Start{};
            Int32 End{};
        };

        struct MsdfData {

            Int32 MaxHeight{};
            TextureHandle TextureAtlas{};
            MTSDFGen MTSDFAtlasInfo{};
            ankerl::unordered_dense::map<msdf_atlas::unicode_t, FontGlyph> GlyphInfo{};
        };

    private:
        auto GenerateAtlas( CStr fontFilename, Int32 fontSize, bool expensiveColoring = true ) const -> MsdfData;
        auto SubmitAtlasBitmapAndLayout(const msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>& atlas,std::vector<msdf_atlas::GlyphGeometry> glyphs, MsdfData& data, Int32 fontSize ) const -> void;

    private:
        GpuDevice *m_GpuDevice{ nullptr };
        msdfgen::FreetypeHandle *m_FreeTypeHandle{ nullptr };
    };
}



#endif //FREETYPEMANAGER_HH
