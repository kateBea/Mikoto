/**
 * OpenGLTexture2D.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_OPENGL_TEXTURE2D_HH
#define MIKOTO_OPENGL_TEXTURE2D_HH

// Third-Party Libraries
#include "stb_image.h"

// Project Headers
#include "Common/Common.hh"
#include "Renderer/Material/Texture2D.hh"

namespace Mikoto {
    class OpenGLTexture2D : public Texture2D {
    public:
        /**
         * Creates a new Texture object and fills it with the data
         * from Texture file in path. If no data is provided it simply creates
         * a valid Texture object with a valid id
         * @param path the path to the Texture file
         * */
        explicit OpenGLTexture2D(const Path_T& path, MapType type, bool retainFileData = false);

        /**
         * Move constructor
         * @param other move from Texture
         * */
        OpenGLTexture2D(OpenGLTexture2D&& other) noexcept;

        /**
         * Move assignment
         * @return *this
         * */
        auto operator=(OpenGLTexture2D&& other) noexcept -> OpenGLTexture2D&;

        MKT_NODISCARD auto GetImGuiTextureHandle() const -> std::any { return m_ObjectID; }

        /**
         * Returns the identifier of this Texture
         * @return id of this object
         * */
        MKT_NODISCARD auto GetId() const -> UInt32_T { return m_ObjectID; }
        MKT_NODISCARD auto GetTextureFileData() const -> stbi_uc* { return m_TextureFileData; }

        static auto Unbind() -> void { glBindTexture(GL_TEXTURE_2D, 0); }
        auto Bind(UInt32_T slot) const -> void;

        ~OpenGLTexture2D();
    public:
        // Forbidden operations
        OpenGLTexture2D(const OpenGLTexture2D& other) = delete;
        auto operator=(const OpenGLTexture2D& other) -> OpenGLTexture2D& = delete;

    private:
        auto SetupTexture(const stbi_uc* data) -> void;

    private:
        UInt32_T m_ObjectID{};          // Object identifier
        GLenum m_InternalFormat{};      // Specifies the sized internal format to be used to store texture image data
        GLenum m_Format{};              // Specifies the format of the pixel data
        stbi_uc* m_TextureFileData{};   // Pointer to the texture buffer data
        bool m_RetainData{};            // Tells whether we want to keep the texture data in the CPU memory or not
    };
}


#endif // MIKOTO_OPENGL_TEXTURE2D_HH
