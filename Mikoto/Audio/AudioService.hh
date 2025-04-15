//
// Created by kate on 1/26/2025.
//

#ifndef AUDIOSYSTEM_HH
#define AUDIOSYSTEM_HH

#include <Assets/Audio.hh>
#include <Common/Service.hh>
#include <Library/Utility/Types.hh>
#include <Threading/Task.hh>

#include "AudioDevice.hh"

namespace Mikoto {
    struct AudioServiceCreateInfo {

    };

    class AudioService final : public IService<AudioService> {
    public:
        explicit AudioService(const AudioServiceCreateInfo& options);

        auto TestSound(const Path_T& soundFile ) const -> void;
        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto GetDevice() -> AudioDevice*;
        MKT_NODISCARD auto GetDevice() const -> const AudioDevice*;

        ~AudioService() override = default;

    private:
        Scope_T<AudioDevice> m_Device{};
    };

}



#endif //AUDIOSYSTEM_HH
