//
// Created by zanet on 3/21/2025.
//

#ifndef IDEVICEOBJECT_HH
#define IDEVICEOBJECT_HH

#include <Library/Data/ResourcePool.hh>
#include <Renderer/GpuUtility.hh>

namespace Mikoto {
    class GpuDevice;

    /**
     * @class DeviceObject
     * @brief Represents a device object associated with a GPU device.
     *
     * This class serves as a base class for GPU resources that are tied to a specific GPU device,
     * such as buffers, textures, and shaders. It manages the association with the device and handles
     * resource allocation and release. The actual allocation and release are implemented by derived classes.
     */
    class DeviceObject : public IResource {
    public:
        /**
         * @brief Default constructor for the DeviceObject class.
         *
         * Initializes a default instance of the DeviceObject. This constructor is typically used
         * when the device object is initialized later with a call to `Init`.
         */
        explicit DeviceObject() = default;

        /**
         * @brief Initializes the device object with the given GPU device.
         *
         * This method sets the device for the object and calls `Allocate` to perform any necessary
         * resource allocation specific to the derived object.
         *
         * @param device A pointer to the GPU device associated with this object.
         */
        auto Initialize( GpuDevice* device ) -> void {
            m_Device = device;

            Allocate();
        }

        auto SetDebugName(const std::string_view name) -> void { m_DebugName = name; }
        auto GetDebugName() const -> const std::string& { return m_DebugName; }

        /**
         * @brief Destructor for the DeviceObject class.
         *
         * Ensures proper cleanup of resources when the device object is destroyed.
         */
        ~DeviceObject() override = default;

        /**
         * @brief Returns the usage type of this device object.
         *
         * @returns The usage type of this device object.
         */
        MKT_NODISCARD auto GetResourceUsage() const -> ResourceUsageType {
            return m_UsageType;
        }

        // API specific object handle
        template<typename ChildType>
        MKT_NODISCARD auto GetNativeHandle() -> decltype(auto) {
            return static_cast<ChildType*>(this)->GetImplHandle();
        }

    protected:

        /***
         * @brief Constructor for the DeviceObject class.
         *
         * Initializes the device object with the given GPU device and resource usage type.
         *
         * @param usageType The resource usage type (e.g., static, dynamic).
         */
        explicit DeviceObject( const ResourceUsageType usageType )
            : m_UsageType{ usageType } {}

        /**
         * @brief Allocates the resources specific to the device object.
         *
         * This method is pure virtual and must be implemented by derived classes to allocate resources
         * (e.g., buffers, textures) on the GPU. It is called when the `Init` method is invoked.
         */
        auto Allocate() -> void override = 0;

        /**
        * @brief Releases the underlying GPU resource.
        *
        * This pure virtual function must be implemented by concrete device resource classes
        * (e.g., buffers, textures, samplers) to handle cleanup of GPU-side memory or API-specific
        * handles. It ensures the resource is properly released from the graphics device.
        *
        * Once called, the object should no longer be used until reinitialized or recreated.
        */
        auto Release() -> void override = 0;

    protected:
        std::string m_DebugName{ "DeviceObject" };
        GpuDevice* m_Device{};
        ResourceUsageType m_UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };
    };
}

#endif//IDEVICEOBJECT_HH
