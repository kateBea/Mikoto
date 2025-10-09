//
// Created by kate on 1/26/2025.
//

#ifndef AUDIOSYSTEM_HH
#define AUDIOSYSTEM_HH

#include <Assets/Audio.hh>
#include <Common/Service.hh>
#include <Library/Utility/Types.hh>

#include "Audio/AudioDevice.hh"

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



#endif //AUDIOSYSTEM_HH
