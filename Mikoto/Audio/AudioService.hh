//    Copyright 2026 ケイト
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

#ifndef MIKOTO_AUDIO_AUDIO_SERVICE_HH
#define MIKOTO_AUDIO_AUDIO_SERVICE_HH

#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioListener.hh>

namespace mikoto::audio {

    /**
     * @brief Configuration used to create the process-wide audio service.
     */
    struct AudioServiceCreateInfo {
        /**
         * @brief Device limits and offline-mode options.
         */
        AudioDeviceDescription mDevice{};
    };

    /**
     * @brief Owns the shared audio device and the pool of reusable listeners.
     */
    class AudioService final : public core::IService, public core::Singleton<AudioService> {
    public:
        /**
         * @brief Stores device configuration for later initialization.
         *
         * @param options Service configuration copied by the constructor.
         */
        explicit AudioService( const AudioServiceCreateInfo& options );


        /**
         * @brief Initializes the device and allocates a fixed listener pool.
         *
         * @note Repeated initialization is a no-op. Slot zero initially acts as the origin
         * listener.
         * @throws core::RuntimeException If backend or HRTF initialization fails.
         */
        auto Initialize() -> void override;


        /**
         * @brief Detaches listeners and shuts down and releases the device.
         *
         * @note Repeated shutdown is a no-op. Outstanding sources and clips are invalidated.
         */
        auto Shutdown() -> void override;


        /**
         * @brief Gets the service-owned audio device.
         *
         * @return Borrowed device pointer, or null before initialization/after shutdown. Do not
         * delete it.
         */
        MKT_NODISCARD auto GetDevice() -> AudioDevice*;


        /**
         * @brief Gets the service-owned audio device.
         *
         * @return Borrowed device pointer, or null before initialization/after shutdown. Do not
         * delete it.
         */
        MKT_NODISCARD auto GetDevice() const -> const AudioDevice*;


        /**
         * @brief Releases a claimed listener slot for reuse.
         *
         * @param listener Pointer returned by CreateListener(); null or unrecognized pointers are
         * ignored.
         * @note Stop using the pointer after release. Releasing the last claimed slot restores the
         * origin listener.
         */
        auto ReleaseListener( AudioListener* listener ) -> void;


        /**
         * @brief Claims a free listener slot and applies its initial position.
         *
         * @param position Initial world-space position.
         * @return Borrowed listener, or null when uninitialized or the pool is exhausted.
         * The pointer is usable until release or shutdown; do not delete it.
         */
        MKT_NODISCARD auto CreateListener( const core::float3& position = {} ) -> AudioListener*;


        /**
         * @brief Destroys the owned device and listener storage.
         *
         * @note All borrowed device/listener pointers become invalid.
         */
        ~AudioService() override = default;


    private:
        eastl::unique_ptr<AudioDevice> mDevice{};

        // On Audio Service initialization we create the fixed amount of listeners
        // and send one at a time whenever CreateListener() is called
        AudioDeviceDescription mDescription{};
        eastl::vector<bool> mListenerUsed{};
        eastl::vector<AudioListener> mListeners{};
    };
}// namespace mikoto::audio

#endif// MIKOTO_AUDIO_AUDIO_SERVICE_HH
