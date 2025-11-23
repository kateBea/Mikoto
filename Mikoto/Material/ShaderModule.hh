/**
 * ShaderModule.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_BASE_SHADER_HH
#define MIKOTO_BASE_SHADER_HH

// C++ Standard Library

// Project Headers
#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>

#include <Renderer/Core/DeviceObject.hh>

namespace Mikoto {

    /**
     * @class ShaderModule
     * @brief Represents a shader resource used in the graphics pipeline.
     * This class encapsulates the properties of a shader, including its stage and the file it was loaded from.
     * Shaders are an essential part of the graphics pipeline and are used for tasks like vertex transformation,
     * pixel coloring, and compute tasks.
     */
    class ShaderModule : public DeviceObject {
    public:

        /**
         * @brief Gets the file associated with the shader.
         * @return A pointer to the file from which the shader was loaded.
         */
        MKT_NODISCARD auto GetContents() const -> const void* {
            return m_Contents;
        }

        /**
         * @brief Gets the size in bytes of the shader contents
         * @return The size in bytes of the shader contents
         */
        MKT_NODISCARD auto GetContentSize() const -> Size {
            return m_ContentsSize;
        }

        /**
         * @brief Gets the file associated with the shader.
         * @return A pointer to the file from which the shader was loaded.
         */
        MKT_NODISCARD auto HasContents() const -> Size {
            return m_Contents != nullptr;
        }

        /**
        * @brief Gets the stage of the shader.
        * @return The stage of the shader (vertex, fragment, or compute).
        */
        MKT_NODISCARD auto GetStage() const -> ShaderStage {
            return m_Stage;
        }

        /**
         * @brief Destructor.
         */
        ~ShaderModule() override = default;

    protected:
        /**
         * @brief Constructs a shader with the specified stage and file.
         * Initializes the shader with the stage (vertex, fragment, compute) and the file from which it is loaded.
         * @param stage The stage of the shader (vertex, fragment, or compute).
         * @param contents Contents for this shader module
         * @param size Size in bytes of for the contents
         */
        explicit ShaderModule( const ShaderStage stage, const void* contents , const Size size)
            : m_Contents{ static_cast<const Byte*>( contents ) }, m_ContentsSize{ size }, m_Stage{ stage } {}

    protected:
        const Byte* m_Contents{ nullptr };

        // Size in bytes
        Size m_ContentsSize{};

        ShaderStage m_Stage{ ShaderStage::STAGE_UNKNOWN };
    };

    using ShaderModuleHandle = Ref<ShaderModule>;

}

#endif// MIKOTO_BASE_SHADER_HH
