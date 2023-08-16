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
#include <GL/glew.h>
#include <volk.h>

// Project Headers
#include <Core/Assert.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    enum class ShaderDataType {
        NONE,
        FLOAT_TYPE,     // Represents a single float data type
        FLOAT2_TYPE,    // Represents a two float data type
        FLOAT3_TYPE,    // Represents a three float data type
        FLOAT4_TYPE,    // Represents a four float data type

        MAT3_TYPE,      // Represents 3x3 float matrix data type
        MAT4_TYPE,      // Represents 4x4 float matrix data type

        INT_TYPE,       // Represents a single int data type
        INT2_TYPE,      // Represents a two int data type
        INT3_TYPE,      // Represents a three int data type
        INT4_TYPE,      // Represents a four int data type
        BOOL_TYPE,      // Represents a single boolean data type
        COUNT,
    };

    /**
     * Represents an data type within a vertex buffer
     * */
    class BufferElement {
    public:
        BufferElement(ShaderDataType type, std::string_view name, bool normalized = false)
            :   m_Name{ name }, m_Type{ type }, m_Size{GetSizeFromShaderType(type) }, m_Offset{ 0 }, m_Normalized{ normalized } {}

        KT_NODISCARD auto GetAttributeCount() const -> UInt32_T { return GetComponentCount(m_Type); }
        KT_NODISCARD auto GetOpenGLAttributeDataType() const -> UInt32_T { return GetOpenGLTypeFromShaderDataType(m_Type); }
        KT_NODISCARD auto GetVulkanAttributeDataType() const -> VkFormat { return GetVulkanTypeFromShaderDataType(m_Type); }
        KT_NODISCARD auto GetAttributeSize() const -> UInt32_T { return m_Size; }

        /*  Getters */
        KT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        KT_NODISCARD auto GetType() const -> ShaderDataType { return m_Type; }
        KT_NODISCARD auto GetSize() const -> UInt32_T { return m_Size; }
        KT_NODISCARD auto GetOffset() const -> UInt32_T { return m_Offset; }
        KT_NODISCARD auto IsNormalized() const  { return m_Normalized; }

        /*  Setters */
        auto SetName(std::string_view name) -> void { m_Name = name; }
        auto SetType(ShaderDataType type) -> void { m_Type = type; }
        auto SetSize(UInt32_T size) -> void { m_Size = size; }
        auto SetOffset(UInt32_T offset) -> void { m_Offset = offset; }
        auto SetNormalized() -> void { m_Normalized = true; }
        auto UnsetNormalized() -> void { m_Normalized = false; }

    private:
        // Size in bytes for integer
        static constexpr UInt32_T s_DefaultShaderIntSize{ 4 };
        // Size in bytes for float
        static constexpr UInt32_T s_DefaultShaderFloatSize{ 4 };

        std::string     m_Name{};
        ShaderDataType  m_Type{};
        UInt32_T        m_Size{};
        UInt32_T        m_Offset{};
        bool            m_Normalized;

        /**
         * Returns the size of the shader data type
         * @returns the size in bytes of the data type
         * */
        static constexpr auto GetSizeFromShaderType(ShaderDataType type) -> UInt32_T {
            switch (type) {
                case ShaderDataType::FLOAT_TYPE:    return s_DefaultShaderFloatSize;
                case ShaderDataType::FLOAT2_TYPE:   return s_DefaultShaderFloatSize * 2;
                case ShaderDataType::FLOAT3_TYPE:   return s_DefaultShaderFloatSize * 3;
                case ShaderDataType::FLOAT4_TYPE:   return s_DefaultShaderFloatSize * 4;

                case ShaderDataType::MAT3_TYPE:     return s_DefaultShaderFloatSize * (3 * 3);
                case ShaderDataType::MAT4_TYPE:     return s_DefaultShaderFloatSize * (4 * 4);

                case ShaderDataType::INT_TYPE:      return s_DefaultShaderIntSize;
                case ShaderDataType::INT2_TYPE:     return s_DefaultShaderIntSize * 2;
                case ShaderDataType::INT3_TYPE:     return s_DefaultShaderIntSize * 3;
                case ShaderDataType::INT4_TYPE:     return s_DefaultShaderIntSize * 4;
                case ShaderDataType::BOOL_TYPE:     return 1;

                case ShaderDataType::NONE:
                case ShaderDataType::COUNT: [[fallthrough]];
                default: KT_ASSERT(false, "Invalid shader data type");
            }

            KT_ASSERT(false, "Invalid shader data type");
        }

        /**
         * Returns the number of components of the given type
         * @returns Count of elements of the data type
         * */
        static constexpr auto GetComponentCount(ShaderDataType type) -> UInt32_T {
            switch(type) {
                case ShaderDataType::FLOAT_TYPE:    return 1;
                case ShaderDataType::FLOAT2_TYPE:   return 2;
                case ShaderDataType::FLOAT3_TYPE:   return 3;
                case ShaderDataType::FLOAT4_TYPE:   return 4;

                case ShaderDataType::MAT3_TYPE:     return 3 * 3;
                case ShaderDataType::MAT4_TYPE:     return 4 * 4;

                case ShaderDataType::INT_TYPE:      return 1;
                case ShaderDataType::INT2_TYPE:     return 2;
                case ShaderDataType::INT3_TYPE:     return 3;
                case ShaderDataType::INT4_TYPE:     return 4;
                case ShaderDataType::BOOL_TYPE:     return 1;

                case ShaderDataType::NONE:
                case ShaderDataType::COUNT: [[fallthrough]];
                default: KT_ASSERT(false, "Invalid shader data type");
            }

            KT_ASSERT(false, "Invalid shader data type");
        }

        static auto GetOpenGLTypeFromShaderDataType(ShaderDataType type) -> GLenum {
            switch(type) {
                case ShaderDataType::FLOAT_TYPE:
                case ShaderDataType::FLOAT2_TYPE:
                case ShaderDataType::FLOAT3_TYPE:
                case ShaderDataType::FLOAT4_TYPE:

                case ShaderDataType::MAT3_TYPE:
                case ShaderDataType::MAT4_TYPE: return GL_FLOAT;

                case ShaderDataType::INT_TYPE:
                case ShaderDataType::INT2_TYPE:
                case ShaderDataType::INT3_TYPE:
                case ShaderDataType::INT4_TYPE:
                case ShaderDataType::BOOL_TYPE: return GL_BOOL;

                case ShaderDataType::NONE:
                case ShaderDataType::COUNT: [[fallthrough]];
                default: KT_ASSERT(false, "Invalid shader data type");
            }
        }

        /**
         * Returns the VkFormat corresponding to the shader data type as follows:
         * float: VK_FORMAT_R32_SFLOAT
         * vec2: VK_FORMAT_R32G32_SFLOAT
         * vec3: VK_FORMAT_R32G32B32_SFLOAT
         * vec4: VK_FORMAT_R32G32B32A32_SFLOAT
         * ivec2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
         * uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
         * double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
         * @param type represents the shader data type
         * */
        static auto GetVulkanTypeFromShaderDataType(ShaderDataType type) -> VkFormat {
            switch(type) {
                case ShaderDataType::FLOAT_TYPE: return VK_FORMAT_R32_SFLOAT;
                case ShaderDataType::FLOAT2_TYPE: return VK_FORMAT_R32G32_SFLOAT;
                case ShaderDataType::FLOAT3_TYPE: return VK_FORMAT_R32G32B32_SFLOAT;
                case ShaderDataType::FLOAT4_TYPE: return VK_FORMAT_R32G32B32A32_SFLOAT;

                case ShaderDataType::MAT3_TYPE: return VK_FORMAT_UNDEFINED; //temporary
                case ShaderDataType::MAT4_TYPE: return VK_FORMAT_UNDEFINED; //temporary

                case ShaderDataType::INT_TYPE:  return VK_FORMAT_R32_SINT;
                case ShaderDataType::INT2_TYPE: return VK_FORMAT_R32G32_SINT;
                case ShaderDataType::INT3_TYPE: return VK_FORMAT_R32G32B32_SINT;
                case ShaderDataType::INT4_TYPE: return VK_FORMAT_R32G32B32A32_SINT;
                case ShaderDataType::BOOL_TYPE: return VK_FORMAT_R32_SINT;

                case ShaderDataType::NONE:
                case ShaderDataType::COUNT: [[fallthrough]];
                default: KT_ASSERT(false, "Invalid shader data type");
            }
        }
    };

    class BufferLayout {
    public:
        BufferLayout(std::initializer_list<BufferElement>&& items)
            :    m_Stride{ 0 }, m_Items(std::forward<std::initializer_list<BufferElement>>(items))
        {
            ComputeOffsetAndStride();
        }

        KT_NODISCARD auto GetElements() const -> const std::vector<BufferElement>& { return m_Items; }
        KT_NODISCARD auto GetCount() const -> Int32_T { return m_Items.size(); }
        KT_NODISCARD auto GetStride() const { return m_Stride; }

        auto operator[](UInt32_T index) -> BufferElement& { return m_Items[index]; }
        auto operator[](UInt32_T index) const -> const BufferElement& { return m_Items[index]; }

        auto begin() -> std::vector<BufferElement>::iterator { return m_Items.begin(); }
        auto end() -> std::vector<BufferElement>::iterator { return m_Items.end(); }

        KT_NODISCARD auto begin() const -> std::vector<BufferElement>::const_iterator { return m_Items.begin(); }
        KT_NODISCARD auto end() const -> std::vector<BufferElement>::const_iterator { return m_Items.end(); }

        auto rbegin() -> std::vector<BufferElement>::reverse_iterator { return m_Items.rbegin(); }
        auto rend() -> std::vector<BufferElement>::reverse_iterator { return m_Items.rend(); }

        KT_NODISCARD auto rbegin() const -> std::vector<BufferElement>::const_reverse_iterator { return m_Items.rbegin(); }
        KT_NODISCARD auto rend() const -> std::vector<BufferElement>::const_reverse_iterator { return m_Items.rend(); }
    private:
        // Helpers
        auto ComputeOffsetAndStride() -> void {
            UInt32_T offset{ 0 };
            for (auto& item : m_Items) {
                item.SetOffset(offset);
                offset += item.GetSize();
                m_Stride += item.GetSize();
            }
        }

        UInt32_T m_Stride{};
        std::vector<BufferElement> m_Items{};
    };

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
         * Sets the buffer layout for the implicit parameter
         * @param layout new layout for this vertex buffer
         * */
        virtual auto SetBufferLayout(const BufferLayout& layout) -> void = 0;

        /**
         * Returns the data layout of this vertex buffer
         * @returns buffer layout of the implicit parameter
         * */
        KT_NODISCARD virtual auto GetBufferLayout() const -> const BufferLayout& = 0;

        /**
         * Returns the total size in bytes of the contents of this Vertex buffer
         * @return total size in bytes of the vertices of this buffer
         * */
        KT_NODISCARD auto GetSize() const -> Size_T { return m_Size; }
        /**
         * Returns the total number of vertices contained within this vertex buffer
         * @return number of vertices
         * */
        KT_NODISCARD virtual auto GetCount() const -> Size_T { return m_Count; }

        /**
         * Returns true if this vertex buffer contains data, false otherwise
         * @returns true if this vertex buffer is empty, false otherwise
         * */
        KT_NODISCARD auto IsEmpty() const -> bool { return m_Size == 0; }

        KT_NODISCARD static auto CreateBuffer(const std::vector<float>& data) -> std::shared_ptr<VertexBuffer>;

        /**
         * Releases the resources of this vertex buffer
         * */
        virtual auto OnRelease() const -> void = 0;

        /**
         * Default destructor
         * */
        virtual ~VertexBuffer() = default;

    public:
        // Note: for now this will always be the same layout as the models, see Model.hh
        static inline BufferLayout s_DefaultBufferLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        KT_NODISCARD static auto GetDefaultBufferLayout() -> const BufferLayout& { return s_DefaultBufferLayout; }
        static auto SetDefaultBufferLayout(const BufferLayout& layout) -> void { s_DefaultBufferLayout = layout; }

    protected:
        UInt64_T m_Size{};
        UInt64_T m_Count{};
    };
}

#endif // MIKOTO_VERTEX_BUFFER_HH