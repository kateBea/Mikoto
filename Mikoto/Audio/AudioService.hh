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

#ifndef MIKOTO_AUDIO_SERVICE_HH
#define MIKOTO_AUDIO_SERVICE_HH

#include <Common/Service.hh>

#include <Library/Utility/Types.hh>

#include <Assets/AudioClip.hh>
#include <Audio/AudioDevice.hh>

namespace Mikoto {
    struct AudioServiceCreateInfo {

    };

    class AudioService final : public IService, public Singleton<AudioService> {
    public:
        explicit AudioService(const AudioServiceCreateInfo& options);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto GetDevice() -> AudioDevice*;
        MKT_NODISCARD auto GetDevice() const -> const AudioDevice*;

        ~AudioService() override = default;

    private:
        Unique<AudioDevice> m_Device{};
    };
}

#endif // MIKOTO_AUDIO_SERVICE_HH
