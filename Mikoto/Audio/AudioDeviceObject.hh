//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_AUDIO_DEVICE_OBJECT_HH
#define MIKOTO_AUDIO_DEVICE_OBJECT_HH

#include <Core/ResourcePool.hh>

namespace mikoto::audio {

    class AudioDevice;

    /**
     * @class AudioDeviceObject
     * @brief Represents a device object associated with a GPU device.
     *
     * This class serves as a base class for GPU resources that are tied to a specific GPU device,
     * such as buffers, textures, and shaders. It manages the association with the device and handles
     * resource allocation and release. The actual allocation and release are implemented by derived classes.
     */
    class AudioDeviceObject : public core::IResource {
    public:
        /**
         * @brief Default constructor for the DeviceObject class.
         * Initializes a default instance of the DeviceObject. This constructor is typically used
         * when the device object is initialized later with a call to `Init`.
         */
        explicit AudioDeviceObject() = default;

        /**
         * @brief Initializes the device object with the given GPU device.
         * This method sets the device for the object and calls `Allocate` to perform any necessary
         * resource allocation specific to the derived object.
         *
         * @param device A pointer to the GPU device associated with this object.
         */
        auto Init( AudioDevice* device ) -> void {
            m_Device = device;
            Initialize();
        }

        /**
         * @brief Destructor for the DeviceObject class.
         * Ensures proper cleanup of resources when the device object is destroyed.
         */
        ~AudioDeviceObject() override = default;

    protected:

        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;

    protected:
        AudioDevice* m_Device{};
    };
}

#endif // MIKOTO_AUDIO_DEVICE_OBJECT_HH
