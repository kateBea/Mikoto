/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_TEXTURE2D_HH
#define MIKOTO_TEXTURE2D_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    enum class Type {
        NONE,
        DIFFUSE,
        SPECULAR,
        NORMAL,
        COUNT,
    };

    class Texture2D {
    public:
        explicit Texture2D() = default;

        MKT_NODISCARD auto GetChannels() const -> UInt32_T { return m_Channels; }
        MKT_NODISCARD auto GetWidth() const -> UInt32_T  { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> UInt32_T { return m_Height; }

        static auto CreateTexture(const Path_T& path) -> std::shared_ptr<Texture2D>;
        static auto CreateTextureRawPtr(const Path_T &path) -> Texture2D*;

        static auto LoadFromFile(const Path_T &path, Type type) -> std::shared_ptr<Texture2D>;

    protected:
        Texture2D(UInt32_T width, UInt32_T height, UInt32_T channels) : m_Width{ width }, m_Height{ height }, m_Channels{ channels } {}

    protected:
        UInt32_T    m_Width{};
        UInt32_T    m_Height{};
        UInt32_T    m_Channels{};
        Type m_Type{};
    };
}

#endif // MIKOTO_TEXTURE2D_HH
