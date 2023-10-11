/**
 * OpenGLTexture2D.hh
 * Created by kate on 6/8/23.
 * */

// Third-Party Libraries
#include <GL/glew.h>
#include <stb_image.h>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Core/Assert.hh>
#include <Renderer/OpenGL/OpenGLTexture2D.hh>

namespace Mikoto {

    OpenGLTexture2D::OpenGLTexture2D(const Path_T &path, MapType type, bool retainFileData)
        :   m_RetainData{ retainFileData }
    {
        // Texture type
        m_Type = type;

        auto fileDir{ GetByteChar(path) };
        stbi_set_flip_vertically_on_load(true);
        m_TextureFileData = stbi_load(fileDir.c_str(), &m_Width, &m_Height, &m_Channels, 4);

        if (m_TextureFileData) {
            glCreateTextures(GL_TEXTURE_2D, 1, &m_ObjectID);

            SetupTexture(m_TextureFileData);

            if (!m_RetainData)
                stbi_image_free(m_TextureFileData);
            else
                m_TextureFileData = nullptr;
        }
        else {
            throw std::runtime_error("Could not load Texture data");
        }

    }

    auto OpenGLTexture2D::Bind(UInt32_T slot) const -> void {
        // TODO: reimplement. glBindTextureUnit requires more recent version of OpenGL
        glBindTextureUnit(slot, m_ObjectID);
    }

    auto OpenGLTexture2D::operator=(OpenGLTexture2D&& other) noexcept -> OpenGLTexture2D& {
        m_ObjectID = other.GetId();
        m_Width     = other.GetWidth();
        m_Height    = other.GetHeight();
        m_Channels  = other.GetChannels();
        m_TextureFileData = other.m_TextureFileData;

        other.m_ObjectID = 0;
        other.m_Width       = 0;
        other.m_Height      = 0;
        other.m_Channels    = 0;

        return *this;
    }

    OpenGLTexture2D::OpenGLTexture2D(OpenGLTexture2D &&other) noexcept
        : Texture2D{ other.m_Width, other.m_Height, other.m_Channels }, m_ObjectID{ other.GetId() }
    {
        other.m_ObjectID = 0;
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
        MKT_ASSERT(m_Format & m_InternalFormat, "Texture data format unsupported");

        glTextureStorage2D(m_ObjectID, 1, m_InternalFormat, m_Width, m_Height);

        glGenerateTextureMipmap(m_ObjectID);
        glTextureParameteri(m_ObjectID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_ObjectID, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTextureParameteri(m_ObjectID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_ObjectID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureSubImage2D(m_ObjectID, 0, 0, 0, m_Width, m_Height, m_Format, GL_UNSIGNED_BYTE, data);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        MKT_APP_LOGGER_INFO("Deleting OpenGLTexture2D. Id {}", GetId());
        glDeleteTextures(1, &m_ObjectID);
    }
}