//
// Created by kate on 6/8/23.
//

#ifndef KATE_ENGINE_OPENGL_TEXTURE2D_HH
#define KATE_ENGINE_OPENGL_TEXTURE2D_HH

#include <stb_image.h>

#include <Utility/Common.hh>

#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class OpenGLTexture2D : public Texture2D {
    public:
        /**
         * Creates a new Texture object and fills it with the data
         * from Texture file in path. If no data is provided it simply creates
         * a valid Texture object with a valid id
         * @param path the path to the Texture file
         * */
        explicit OpenGLTexture2D(const Path_T& path, bool retainFileData = false);

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

        /**
         * Returns the identifier of this Texture
         * @return id of this object
         * */
        KT_NODISCARD auto GetId() const -> UInt32_T override { return m_Id; }

        KT_NODISCARD auto GetChannels() const -> UInt32_T override { return m_Channels; }
        KT_NODISCARD auto GetWidth() const -> UInt32_T override { return m_Width; }
        KT_NODISCARD auto GetHeight() const -> UInt32_T override { return m_Height; }

        KT_NODISCARD auto GetTextureFileData() const -> stbi_uc* { return m_TextureFileData; }

        auto Unbind() -> void;
        auto Bind(UInt32_T slot) -> void;

        ~OpenGLTexture2D() override;
    public:
        // Forbidden operations
        OpenGLTexture2D(const OpenGLTexture2D& other) = delete;
        auto operator=(const OpenGLTexture2D& other) -> OpenGLTexture2D& = delete;

    private:
        auto SetupTexture(const stbi_uc* data) -> void;

        UInt32_T    m_Id{};
        UInt32_T    m_Width{};
        UInt32_T    m_Height{};
        UInt32_T    m_Channels{};

        GLenum      m_InternalFormat{}; // Specifies the sized internal format to be used to store texture image data
        GLenum      m_Format{};         // Specifies the format of the pixel data

        bool m_RetainData{};
        stbi_uc* m_TextureFileData{};
    };
}


#endif//KATE_ENGINE_OPENGL_TEXTURE2D_HH
