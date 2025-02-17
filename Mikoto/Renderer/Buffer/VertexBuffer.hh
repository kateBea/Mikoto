/**
 * VertexBuffer.HH
 * Created by kate on 6/5/23.
 * */
#ifndef MIKOTO_VERTEX_BUFFER_HH
#define MIKOTO_VERTEX_BUFFER_HH

// C++ Standard Library
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Models/BufferLayout.hh>
#include <Models/Enums.hh>

namespace Mikoto {
    struct VertexBufferCreateInfo {
        std::vector<float> Data{};
        BufferLayout Layout{};
        bool RetainData{};
    };

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
        explicit VertexBuffer( BufferLayout layout ) : m_Layout{ std::move( layout ) } {}

        /**
         * Returns the data layout of this vertex buffer
         * @returns buffer layout of the implicit parameter
         * */
        MKT_NODISCARD auto GetBufferLayout() const -> const BufferLayout& { return m_Layout; }

        /**
         * Sets the buffer layout for the implicit parameter
         * @param layout new layout for this vertex buffer
         * */
        auto SetBufferLayout( const BufferLayout& layout ) -> void { m_Layout = layout; }

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
        MKT_NODISCARD static auto Create( const std::vector<float>& data, const BufferLayout& layout = GetDefaultBufferLayout() ) -> Scope_T<VertexBuffer>;

        MKT_NODISCARD static auto GetDefaultBufferLayout() -> const BufferLayout& { return s_DefaultBufferLayout; }

        /**
         * Default destructor
         * */
        virtual ~VertexBuffer() = default;

    protected:
        // Note: for now this will always be the same layout as the Models, see Model.hh
        static inline BufferLayout s_DefaultBufferLayout{
            { ShaderDataType::FLOAT3_TYPE, "a_Position" },
            { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
            { ShaderDataType::FLOAT3_TYPE, "a_Color" },
            { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        UInt64_T m_Size{};
        UInt64_T m_Count{};
        BufferLayout m_Layout{};
    };
}

#endif// MIKOTO_VERTEX_BUFFER_HH