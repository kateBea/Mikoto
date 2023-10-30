/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_TEXTURE2D_HH
#define MIKOTO_TEXTURE2D_HH

// C++ Standard Library
#include <any>
#include <memory>

// Project Headers
#include "Common/Common.hh"
#include "Common/Types.hh"

namespace Mikoto {
    enum class MapType {
        TEXTURE_2D_INVALID,
        TEXTURE_2D_DIFFUSE,
        TEXTURE_2D_SPECULAR,
        TEXTURE_2D_EMISSIVE,
        TEXTURE_2D_NORMAL,
        TEXTURE_2D_COUNT,
    };

    class Texture2D {
    public:
        MKT_NODISCARD auto GetChannels() const -> Int32_T { return m_Channels; }
        MKT_NODISCARD auto GetWidth() const -> Int32_T  { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }
        MKT_NODISCARD auto GetType() const -> MapType { return m_Type; }

        MKT_NODISCARD virtual auto GetImGuiTextureHandle() const -> std::any = 0;

        static auto Create(const Path_T& path, MapType type) -> std::shared_ptr<Texture2D>;

        virtual ~Texture2D() = default;
    protected:
        explicit Texture2D(MapType map)
            :   m_Width{}, m_Height{}, m_Channels{}, m_Type{ map }
        {

        }

        Texture2D(Int32_T width, Int32_T height, Int32_T channels)
            :   m_Width{ width }, m_Height{ height }, m_Channels{ channels }
        {

        }

    protected:
        Int32_T m_Width{};
        Int32_T m_Height{};
        Int32_T m_Channels{};
        MapType m_Type{};
    };
}

#endif // MIKOTO_TEXTURE2D_HH
