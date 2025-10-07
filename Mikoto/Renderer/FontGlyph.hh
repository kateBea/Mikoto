//
// Created by zanet on 3/15/2025.
//

#ifndef GLYPH_HH
#define GLYPH_HH

#include <glm/glm.hpp>

#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/BufferLayout.hh>

#include "Renderer/GpuDevice.hh"

namespace Mikoto {

    // Represents the specific characters of a given font
    class FontGlyph final {
    public:

        explicit FontGlyph();

        MKT_NODISCARD static auto GetDefaultBufferLayout() -> const BufferLayout&;

        MKT_NODISCARD auto IsSpace() const -> bool;
        MKT_NODISCARD auto IsLineFeed() const -> bool;

        MKT_NODISCARD auto GetSize() const -> const glm::ivec2& { return m_Size; }
        MKT_NODISCARD auto GetBearing() const -> const glm::ivec2& { return m_Bearing; }
        MKT_NODISCARD auto GetAdvance() const -> UInt32 { return m_Advance; }

        MKT_NODISCARD auto GetTexture() const -> BufferHandle;
        MKT_NODISCARD auto GetVertexBuffer() const -> BufferHandle;
        MKT_NODISCARD auto GetIndexBuffer() const -> BufferHandle;

    private:

        auto CreateBuffers() -> void;

    private:
        static inline BufferLayout s_DefaultBufferLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        glm::ivec2   m_Size{};
        glm::ivec2   m_Bearing{};
        UInt32 m_Advance{};

        BufferHandle m_VertexBuffer{};
        BufferHandle m_IndexBuffer{};
    };
}



#endif //GLYPH_HH
