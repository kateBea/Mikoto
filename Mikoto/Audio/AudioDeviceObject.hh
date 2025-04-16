//
// Created by zanet on 3/28/2025.
//

#ifndef AUDIODEVICEOBJECT_HH
#define AUDIODEVICEOBJECT_HH

#include <Library/Data/ResourcePool.hh>

namespace Mikoto {

    class AudioDevice;

    /**
     * @class AudioDeviceObject
     * @brief Represents a device object associated with a GPU device.
     *
     * This class serves as a base class for GPU resources that are tied to a specific GPU device,
     * such as buffers, textures, and shaders. It manages the association with the device and handles
     * resource allocation and release. The actual allocation and release are implemented by derived classes.
     */
    class AudioDeviceObject : public IResource {
    public:
        /**
         * @brief Default constructor for the DeviceObject class.
         *
         * Initializes a default instance of the DeviceObject. This constructor is typically used
         * when the device object is initialized later with a call to `Init`.
         */
        explicit AudioDeviceObject() = default;

        /**
         * @brief Initializes the device object with the given GPU device.
         *
         * This method sets the device for the object and calls `Allocate` to perform any necessary
         * resource allocation specific to the derived object.
         *
         * @param device A pointer to the GPU device associated with this object.
         */
        auto Init( AudioDevice* device ) -> void {
            m_Device = device;

            Allocate();
        }

        /**
         * @brief Destructor for the DeviceObject class.
         *
         * Ensures proper cleanup of resources when the device object is destroyed.
         */
        ~AudioDeviceObject() override = default;

    protected:
        /**
         * @brief Allocates the resources specific to the device object.
         *
         * This method is pure virtual and must be implemented by derived classes to allocate resources
         * (e.g., buffers, textures) on the GPU. It is called when the `Init` method is invoked.
         */
        auto Allocate() -> void override = 0;

        auto Release() -> void override = 0;

    protected:
        AudioDevice* m_Device{};
    };
}

#endif //AUDIODEVICEOBJECT_HH
