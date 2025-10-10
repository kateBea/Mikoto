//
// Created by zanet on 3/27/2025.
//

#ifndef BUFFER_HH
#define BUFFER_HH

#include <Library/Utility/Types.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/GpuUtility.hh>

namespace Mikoto {

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
        virtual auto CopyFromBlock(const void* ptr, Size size) -> void = 0;

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
        Buffer( Byte* data, const Size sizeBytes, const BufferUsage usage, const ResourceUsageType usageType )
            : DeviceObject{ usageType }, m_Data{ data}, m_SizeBytes{ sizeBytes }, m_Usage{ usage } {}

    protected:
        Byte* m_Data{ nullptr };
        Size m_SizeBytes{};
        BufferDataType m_DataType{ BufferDataType::BUFFER_DATA_TYPE_UNKNOWN };
        BufferUsage m_Usage{ BufferUsage::BUFFER_USAGE_VERTEX };
    };

    using BufferHandle = Ref<Buffer>;
}// namespace Mikoto
#endif//BUFFER_HH
