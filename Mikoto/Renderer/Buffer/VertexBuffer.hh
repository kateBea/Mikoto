/**
 * VertexBuffer.cc
 * Created by kate on 6/5/23.
 * */
#ifndef MIKOTO_VERTEX_BUFFER_HH
#define MIKOTO_VERTEX_BUFFER_HH

// C++ Standard Library
#include <vector>
#include <string>
#include <string_view>
#include <utility>
#include <memory>

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include <Common/Constants.hh>
#include <Models/Enums.hh>

#include "Common/Common.hh"
#include "Core/Assert.hh"
#include "Models/BufferLayout.hh"

namespace Mikoto {

    /**
     * General abstraction around blocks of memory that the
     * GPU can see and write/read to.
     * */
    class VertexBuffer {
    public:
        /**
         * Default constructor
         * */
        explicit VertexBuffer() = default;

        /**
         * Creates and initializes a new vertex buffer with the layout provided
         * @param layout vertex buffer layout
         * */
        explicit VertexBuffer(BufferLayout&& layout) : m_Layout{ std::move(layout) } {}

        /**
         * Returns the data layout of this vertex buffer
         * @returns buffer layout of the implicit parameter
         * */
        MKT_NODISCARD auto GetBufferLayout() const -> const BufferLayout& { return m_Layout; }

        /**
         * Sets the buffer layout for the implicit parameter
         * @param layout new layout for this vertex buffer
         * */
        auto SetBufferLayout(const BufferLayout& layout) -> void { m_Layout = layout; }

        /**
         * Returns the total size in bytes of the contents of this Vertex buffer
         * @return total size in bytes of the vertices of this buffer
         * */
        MKT_NODISCARD auto GetSize() const -> Size_T { return m_Size; }
        /**
         * Returns the total number of vertices within this vertex buffer
         * @return number of vertices
         * */
        MKT_NODISCARD auto GetCount() const -> Size_T { return m_Count; }

        /**
         * Returns true if this vertex buffer contains data, false otherwise
         * @returns true if this vertex buffer is empty, false otherwise
         * */
        MKT_NODISCARD auto IsEmpty() const -> bool { return m_Size == 0; }

        /**
         * Creates a vertex buffer with the specified layout
         * @returns pointer to the newly created buffer
         * */
        MKT_NODISCARD static auto Create(const std::vector<float>& data, const BufferLayout& layout) -> std::shared_ptr<VertexBuffer>;

        /**
         * Default destructor
         * */
        virtual ~VertexBuffer() = default;

    public:
        // Note: for now this will always be the same layout as the Models, see Model.hh
        static inline BufferLayout s_DefaultBufferLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        MKT_NODISCARD static auto GetDefaultBufferLayout() -> const BufferLayout& { return s_DefaultBufferLayout; }
        static auto SetDefaultBufferLayout(const BufferLayout& layout) -> void { s_DefaultBufferLayout = layout; }

    protected:
        UInt64_T m_Size{};
        UInt64_T m_Count{};
        BufferLayout m_Layout{};
    };
}

#endif // MIKOTO_VERTEX_BUFFER_HH