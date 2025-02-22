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
#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Library/Filesystem/File.hh>
#include <Library/Random/Random.hh>
#include <Library/Utility/Types.hh>
#include <Models/Enums.hh>

namespace Mikoto {

    class Texture2D : public Texture {
    public:
        MKT_NODISCARD auto GetChannels() const -> Int32_T { return m_Channels; }
        MKT_NODISCARD auto GetWidth() const -> Int32_T  { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }
        MKT_NODISCARD auto GetType() const -> MapType { return m_Type; }
        MKT_NODISCARD auto GetFile() const -> const File* { return m_File; }

        /**
         * @brief Creates a new texture
         * Creates a Texture2D based on the active graphics API (Vulkan or OpenGL) from the provided file path and MapType.
         * @param path The path to the texture file.
         * @param type The MapType for the texture.
         * @return A shared pointer to the created Texture2D. If creation fails, returns a null pointer.
         * */
        MKT_NODISCARD static auto Create(const Path_T& path, MapType type) -> Scope_T<Texture2D>;

        ~Texture2D() override = default;

    protected:
        explicit Texture2D( const MapType map)
            :   m_Type{ map }
        {

        }

        Texture2D( const Int32_T width, const Int32_T height, const Int32_T channels)
            :   m_Width{ width }, m_Height{ height }, m_Channels{ channels }
        {

        }

    protected:
        Int32_T m_Width{};
        Int32_T m_Height{};
        Int32_T m_Channels{};

        MapType m_Type{};

        const File* m_File{ nullptr };
    };
}

#endif // MIKOTO_TEXTURE2D_HH
