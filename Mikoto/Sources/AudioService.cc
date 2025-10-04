//
// Created by zanet on 1/26/2025.
//

#include <Audio/AudioService.hh>

namespace Mikoto {


    AudioService::AudioService( const AudioServiceCreateInfo &options ) {
    }

    auto AudioService::GetDevice() -> AudioDevice * {
        return m_Device.get();
    }

    auto AudioService::GetDevice() const -> const AudioDevice * {
        return m_Device.get();
    }

    auto AudioService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing AudioService...");

        constexpr AudioDeviceDescription description{};
        m_Device = AudioDevice::Create( description );

        if (m_Device) {
            m_Device->Init();
        }

        m_IsInitialized = true;
    }

    auto AudioService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down AudioService..." );

        m_Device->Shutdown();
        m_Device = nullptr;
    }

}// namespace Mikoto