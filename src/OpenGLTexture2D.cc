//
// Created by kate on 6/8/23.
//

#include <GL/glew.h>

#include <stb_image.h>

#include <Utility/Common.hh>

#include <Core/Logger.hh>
#include <Core/Assert.hh>

#include <Renderer/OpenGL/OpenGLTexture2D.hh>

namespace kaTe {

    OpenGLTexture2D::OpenGLTexture2D(const Path_T &path, bool retainFileData)
        :   m_RetainData{ retainFileData }
    {
        // STB image expects width and height and channel to be signed integers
        Int32_T width{}, height{}, channels{};

        auto fileDir{ GetByteChar(path) };
        stbi_set_flip_vertically_on_load(true);
        m_TextureFileData = stbi_load(fileDir.c_str(), &width, &height, &channels, 4);

        if (m_TextureFileData) {
            m_Width = width;
            m_Height = height;
            m_Channels = channels;
            glCreateTextures(GL_TEXTURE_2D, 1, &m_Id);
            SetupTexture(m_TextureFileData);

            // free the data if we do not want to keep it
            if (!m_RetainData)
                stbi_image_free(m_TextureFileData);
            else
                m_TextureFileData = nullptr;
        }
        else {
            throw std::runtime_error("Could not load Texture data");
        }

    }

    auto OpenGLTexture2D::Bind(UInt32_T slot) -> void {
        glBindTextureUnit(slot, m_Id);
    }

    auto OpenGLTexture2D::operator=(OpenGLTexture2D&& other) noexcept -> OpenGLTexture2D& {
        m_Id        = other.GetId();
        m_Width     = other.GetWidth();
        m_Height    = other.GetHeight();
        m_Channels  = other.GetChannels();
        m_TextureFileData = other.m_TextureFileData;

        other.m_Id          = 0;
        other.m_Width       = 0;
        other.m_Height      = 0;
        other.m_Channels    = 0;

        return *this;
    }

    OpenGLTexture2D::OpenGLTexture2D(OpenGLTexture2D &&other) noexcept
        :   m_Id{ other.GetId() }, m_Width{other.GetWidth() }, m_Height{other.GetHeight() }
        ,   m_Channels{other.GetChannels() }, m_TextureFileData{ other.m_TextureFileData }
    {
        other.m_Id          = 0;
        other.m_Width       = 0;
        other.m_Height      = 0;
        other.m_Channels    = 0;
    }

    auto OpenGLTexture2D::SetupTexture(const stbi_uc* data) -> void {
        switch (m_Channels) {
            case 3:
                m_Format = GL_RGB;
                m_InternalFormat = GL_RGB8;
                break;
            case 4:
                m_Format = GL_RGBA;
                m_InternalFormat = GL_RGBA8;
                break;
        }
        KT_ASSERT(m_Format & m_InternalFormat, "Texture data format unsupported");

        glTextureStorage2D(m_Id, 1, m_InternalFormat, m_Width, m_Height);

        glGenerateTextureMipmap(m_Id);
        glTextureParameteri(m_Id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_Id, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTextureParameteri(m_Id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_Id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureSubImage2D(m_Id, 0, 0, 0, m_Width, m_Height, m_Format, GL_UNSIGNED_BYTE, data);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        KATE_APP_LOGGER_INFO("Deleting OpenGLTexture2D. Id {}", GetId());
        glDeleteTextures(1, &m_Id);
    }
}