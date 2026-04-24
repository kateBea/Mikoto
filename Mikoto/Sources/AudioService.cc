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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>

#include <Audio/AudioService.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    AudioService::AudioService( const AudioServiceCreateInfo &options ) {
    }

    auto AudioService::GetDevice() -> AudioDevice * {
        return mDevice.get();
    }

    auto AudioService::GetDevice() const -> const AudioDevice * {
        return mDevice.get();
    }

    auto AudioService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing AudioService...");

        constexpr AudioDeviceDescription description{};
        mDevice = AudioDevice::Create( description );

        if (mDevice) {
            mDevice->Init();
        }

        // Initialize default listeners
        for ( i32 index{}; index < description.mMaxListeners; ++index ) {
            mListeners.emplace_back( index, 0.0f, 0.0f, 0.0f );
        }

        mIsInitialized = true;
    }

    auto AudioService::CreateListener() -> AudioListener * {
        MKT_ASSERT( mCurrentAllocationCount < mListeners.size(), "Reached max number of listeners" );
        return MKT_ADDRESSOF( mListeners[mCurrentAllocationCount++] );
    }

    auto AudioService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down AudioService..." );

        mDevice->Shutdown();
        mDevice.reset();
    }
}// namespace Mikoto