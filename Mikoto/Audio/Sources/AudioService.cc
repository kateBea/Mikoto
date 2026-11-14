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

#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Audio/AudioService.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    AudioService::AudioService( const AudioServiceCreateInfo& options )
        : mDescription{ options.mDevice } {
    }

    auto AudioService::GetDevice() -> AudioDevice* {
        MKT_BEGIN_PROFILER();
        return mDevice.get();
    }

    auto AudioService::GetDevice() const -> const AudioDevice* {
        MKT_BEGIN_PROFILER();
        return mDevice.get();
    }

    auto AudioService::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        mDevice = AudioDevice::Create( mDescription );
        mDevice->Initialize();

        const core::u32 count{ mDevice->GetListenerCount() };
        mListeners.clear();
        mListeners.reserve( count );
        mListenerUsed.clear();
        mListenerUsed.resize( count, false );

        for ( core::u32 i{}; i < count; ++i ) {
            mListeners.emplace_back( core::as<core::i32>( i ), 0.0f, 0.0f, 0.0f, mDevice.get() );
            mListeners.back().SetEnabled( i == 0 );// Default origin listener until an entity claims a slot.
        }

        mIsInitialized = true;
    }

    auto AudioService::CreateListener( const core::float3& position ) -> AudioListener* {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return nullptr;
        }

        for ( core::usize i{}; i < mListeners.size(); ++i ) {
            if ( !mListenerUsed[i] ) {
                mListenerUsed[i] = true;
                mListeners[i] = AudioListener{ core::as<core::i32>( i ),
                    position.x, position.y, position.z, mDevice.get() };
                mListeners[i].Apply();

                // Safe to do as listeners vector never gets reallocated
                // Iterators should still be valid until shutdown
                return MKT_ADDRESSOF( mListeners[i] );
            }
        }

        MKT_CORE_LOGGER_WARN( "No free audio listeners (maximum {}). Remove an unused listener component to release a slot.", mListeners.size() );

        return nullptr;
    }

    auto AudioService::ReleaseListener( AudioListener* listener ) -> void {
        MKT_BEGIN_PROFILER();

        for ( core::usize i{}; i < mListeners.size(); ++i ) {
            if ( listener == &mListeners[i] && mListenerUsed[i] ) {
                mListenerUsed[i] = false;
                mListeners[i].SetEnabled( false );

                if ( eastl::none_of( mListenerUsed.begin(), mListenerUsed.end(),
                         []( bool used ) {
                    return used;
                } ) ) {
                    mListeners[0] = AudioListener{ 0, 0, 0, 0, mDevice.get() };
                    mListeners[0].Apply();
                }

                return;
            }
        }
    }

    auto AudioService::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        for ( auto& listener: mListeners ) {
            listener.Detach();
        }

        mListenerUsed.clear();
        mListenerUsed.resize( mListeners.size(), false );

        if ( mDevice ) {
            mDevice->Shutdown();
            mDevice.reset();
        }

        mIsInitialized = false;
    }
}// namespace mikoto::audio
