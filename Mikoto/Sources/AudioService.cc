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

#include <Core/Profiler.hh>
#include <Memory/Allocator.hh>
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
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing AudioService...");

        constexpr AudioDeviceDescription description{};
        m_Device = AudioDevice::Create( description );

        if (m_Device) {
            m_Device->Init();
        }

        // Initialize default listeners
        for ( Size index{}; index < description.MaxListenersCount; ++index ) {
            m_Listeners.emplace_back( index, 0.0f, 0.0f, 0.0f );
        }

        m_IsInitialized = true;
    }

    auto AudioService::CreateListener() -> AudioListener * {
        MKT_ASSERT( m_CurrentAllocationCount < m_Listeners.size(), "Reached max number of listeners" );
        return MKT_ADDRESSOF( m_Listeners[m_CurrentAllocationCount++] );
    }

    auto AudioService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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