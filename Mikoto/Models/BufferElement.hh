//
// Created by kate on 1/4/25.
//

#ifndef BUFFERELEMENT_HH
#define BUFFERELEMENT_HH

#include <Common/Common.hh>
#include <Common/Constants.hh>
#include <Core/Logging/Assert.hh>
#include <Models/Enums.hh>

namespace Mikoto {
    /**
     * Represents an data type within a vertex buffer
     * */
    class BufferElement {
    public:
        BufferElement(ShaderDataType type, std::string_view name, bool normalized = false)
            :   m_Name{ name }, m_Type{ type }, m_Size{GetSizeFromShaderType(type) }, m_Offset{ 0 }, m_Normalized{ normalized } {}

        MKT_NODISCARD auto GetAttributeCount() const -> UInt32_T { return GetComponentCount(m_Type); }
        MKT_NODISCARD auto GetAttributeSize() const -> UInt32_T { return m_Size; }

        /*  Getters */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetType() const -> ShaderDataType { return m_Type; }
        MKT_NODISCARD auto GetSize() const -> UInt32_T { return m_Size; }
        MKT_NODISCARD auto GetOffset() const -> UInt32_T { return m_Offset; }
        MKT_NODISCARD auto IsNormalized() const  { return m_Normalized; }

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
                default:
                    MKT_ASSERT(false, "Invalid shader data type");
            }

            MKT_ASSERT(false, "Invalid shader data type");
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
                default:
                    MKT_ASSERT(false, "Invalid shader data type");
            }

            MKT_ASSERT(false, "Invalid shader data type");
        }
    };
}

#endif //BUFFERELEMENT_HH
