//
// Created by zanet on 3/27/2025.
//

#ifndef BUFFER_HH
#define BUFFER_HH

#include <initializer_list>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/RenderUtility.hh>
#include <Renderer/RenderUtility.hh>

namespace Mikoto {

    /**
     * Represents an data type within a vertex buffer
     * */
    class BufferElement {
    public:
        BufferElement(ShaderDataType type, std::string_view name, bool normalized = false)
            :   m_Name{ name }, m_Type{ type }, m_Size{GetSizeFromShaderType(type) }, m_Offset{ 0 }, m_Normalized{ normalized } {}

        MKT_NODISCARD auto GetAttributeCount() const -> UInt32 { return GetComponentCount(m_Type); }
        MKT_NODISCARD auto GetAttributeSize() const -> UInt32 { return m_Size; }

        /*  Getters */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetType() const -> ShaderDataType { return m_Type; }
        MKT_NODISCARD auto GetSize() const -> UInt32 { return m_Size; }
        MKT_NODISCARD auto GetOffset() const -> UInt32 { return m_Offset; }
        MKT_NODISCARD auto IsNormalized() const  { return m_Normalized; }

        /*  Setters */
        auto SetName(std::string_view name) -> void { m_Name = name; }
        auto SetType(ShaderDataType type) -> void { m_Type = type; }
        auto SetSize(UInt32 size) -> void { m_Size = size; }
        auto SetOffset(UInt32 offset) -> void { m_Offset = offset; }
        auto SetNormalized() -> void { m_Normalized = true; }
        auto UnsetNormalized() -> void { m_Normalized = false; }

    private:
        // Size in bytes for integer
        static constexpr UInt32 s_DefaultShaderIntSize{ 4 };
        // Size in bytes for float
        static constexpr UInt32 s_DefaultShaderFloatSize{ 4 };

        std::string     m_Name{};
        ShaderDataType  m_Type{};
        UInt32        m_Size{};
        UInt32        m_Offset{};
        bool            m_Normalized;

        /**
         * Returns the size of the shader data type
         * @returns the size in bytes of the data type
         * */
        static constexpr auto GetSizeFromShaderType(ShaderDataType type) -> UInt32 {
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

                case ShaderDataType::NONE:[[fallthrough]];
                case ShaderDataType::COUNT:
                    break;
            }

            return 0;
        }

        /**
         * Returns the number of components of the given type
         * @returns Count of elements of the data type
         * */
        static constexpr auto GetComponentCount(ShaderDataType type) -> UInt32 {
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

                case ShaderDataType::NONE:[[fallthrough]];
                case ShaderDataType::COUNT:
                    break;
            }

            return 0;
        }
    };

    class BufferLayout {
    public:
        BufferLayout( std::initializer_list<BufferElement>&& items)
            : m_Items(std::forward<std::initializer_list<BufferElement>>(items))
        {
            ComputeOffsetAndStride();
        }

        MKT_NODISCARD auto HasElements() const -> bool { return m_Items.size() != 0; }
        MKT_NODISCARD auto GetElements() const -> const std::vector<BufferElement>& { return m_Items; }
        MKT_NODISCARD auto GetCount() const -> Size { return m_Items.size(); }
        MKT_NODISCARD auto GetStride() const { return m_Stride; }

        auto operator[]( const UInt32 index) -> BufferElement& { return m_Items[index]; }
        auto operator[]( const UInt32 index) const -> const BufferElement& { return m_Items[index]; }

        auto begin() -> std::vector<BufferElement>::iterator { return m_Items.begin(); }
        auto end() -> std::vector<BufferElement>::iterator { return m_Items.end(); }

        MKT_NODISCARD auto begin() const -> std::vector<BufferElement>::const_iterator { return m_Items.begin(); }
        MKT_NODISCARD auto end() const -> std::vector<BufferElement>::const_iterator { return m_Items.end(); }

        auto rbegin() -> std::vector<BufferElement>::reverse_iterator { return m_Items.rbegin(); }
        auto rend() -> std::vector<BufferElement>::reverse_iterator { return m_Items.rend(); }

        MKT_NODISCARD auto rbegin() const -> std::vector<BufferElement>::const_reverse_iterator { return m_Items.rbegin(); }
        MKT_NODISCARD auto rend() const -> std::vector<BufferElement>::const_reverse_iterator { return m_Items.rend(); }
    private:
        // Helpers
        auto ComputeOffsetAndStride() -> void {
            UInt32 offset{ 0 };

            for (BufferElement& bufferElement : m_Items) {
                bufferElement.SetOffset(offset);
                offset += bufferElement.GetSize();
                m_Stride += bufferElement.GetSize();
            }
        }

    private:
        // The size in bytes for all the elements contained
        // within this buffer layout, e.g: if this buffer layout has
        // 2 buffer elements (1 float and 1 mat4), m_Stride = Size(float) + Size(mat4)
        // where Size yields the size in bytes of the element, see `BufferElement` for sizes
        UInt32 m_Stride{};

        // The buffer elements
        std::vector<BufferElement> m_Items{};
    };

    /**
     * @class Device
     * @brief Represents a generic buffer used for storing data.
     *
     * This class encapsulates buffer properties such as its size, usage type, and resource usage.
     * Buffers are often used in graphics and compute pipelines for storing vertices, indices,
     * and other data that can be transferred between the CPU and GPU. This class is designed
     * to be inherited by other classes that require specific buffer behavior.
     */
    class Buffer : public DeviceObject {
    public:
        /**
         * @brief Gets the size of the buffer in bytes.
         *
         * @return The size of the buffer in bytes.
         */
        MKT_NODISCARD auto GetSizeBytes() const -> Size {
            return m_SizeBytes;
        }

        /**
         * @brief Gets the usage type of the buffer.
         *
         * @return The buffer usage type (e.g., vertex buffer).
         */
        MKT_NODISCARD auto GetUsage() const -> BufferUsage {
            return m_Usage;
        }

        /**
         * @brief Gets the type of data contained in the buffer
         *
         * @return The type of data in the buffer (float32, int32, etc.).
         */
        MKT_NODISCARD auto GetDataType() const -> BufferDataType {
            return m_DataType;
        }

        // Copy from CPU to GPU ( this buffer must be accessible from CPU)
        virtual auto CopyToBlock( void* ptr, Size size ) -> void = 0;
        virtual auto CopyFromBlock(const void* ptr, Size size) -> void = 0;
        virtual auto CopyFromBlock(const void* ptr, Size size, Size offset) -> void = 0;

        MKT_NODISCARD auto GetCount() const -> Size {
            return InferElementCount(m_DataType, m_SizeBytes);
        }

        using DeviceObject::Initialize;

    protected:
        /**
         * @brief Protected constructor for the Device class.
         *
         * Initializes the buffer with the provided size, usage type, and resource usage type.
         *
         * @param sizeBytes The size of the buffer in bytes.
         * @param usage The usage type of the buffer (e.g., vertex, index).
         * @param usageType The resource usage type (e.g., static, dynamic).
         */
        Buffer( Byte* data, const Size sizeBytes, const BufferUsage usage, const ResourceUsageType usageType, BufferDataType dataType )
            : DeviceObject{ usageType }, m_Data{ data}, m_SizeBytes{ sizeBytes }, m_DataType{dataType}, m_Usage{ usage } {}

    protected:
        Byte* m_Data{ nullptr };
        Size m_SizeBytes{};
        BufferDataType m_DataType{ BufferDataType::BUFFER_DATA_TYPE_UNKNOWN };
        BufferUsage m_Usage{ BufferUsage::BUFFER_USAGE_VERTEX };
    };

    using BufferHandle = Ref<Buffer>;
}// namespace Mikoto
#endif//BUFFER_HH
