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

#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioMixer.hh>
#include <Audio/AudioBackend.hh>

namespace mikoto::audio {

    AudioMixer::AudioMixer( AudioDevice* device )
        : mImpl{ eastl::make_unique<Impl>() }, mDevice{ device } {
    }

    auto AudioMixer::Initialize() -> void {
        if ( mIsInitialized || !mDevice || !mDevice->IsInitialized() ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };

        // Reserve registration before creating a bus so allocation failure leaves no native resource.
        auto& mixers{ mDevice->mImpl->mLiveMixers };
        mixers.reserve( mixers.size() + 1 );

        if ( ma_sound_group_init( &mDevice->mImpl->mAudioEngine,
                 MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &mImpl->mMaster ) != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "AudioMixer::Initialize could not create the master bus." );
        }

        mixers.push_back( this );
        mIsInitialized = true;
    }

    AudioMixer::~AudioMixer() {
        Shutdown();
    }

    auto AudioMixer::SetVolume( core::f32 volume, bool muted ) -> void {
        if ( mIsInitialized ) {
            ma_sound_group_set_volume( &mImpl->mMaster, muted ? 0.f : AudioValue( volume, 0, 20 ) );
        }
    }

    auto AudioMixer::BeginUpdate() -> void {
        for ( auto& [id, group]: mImpl->mGroups ) {
            group->mUsed = false;
        }
    }

    auto AudioMixer::UpdateGroup( core::u64 id, core::f32 volume, bool muted ) -> bool {
        if ( !mIsInitialized || !id ) {
            return false;
        }

        auto it{ mImpl->mGroups.find( id ) };

        if ( it == mImpl->mGroups.end() ) {
            auto group{ eastl::make_unique<Impl::Group>() };

            if ( ma_sound_group_init( &mDevice->mImpl->mAudioEngine, MA_SOUND_FLAG_NO_SPATIALIZATION, &mImpl->mMaster, &group->mNode ) != MA_SUCCESS ) {
                return false;
            }

            it = mImpl->mGroups.emplace( id, eastl::move( group ) ).first;
        }

        it->second->mUsed = true;
        ma_sound_group_set_volume( &it->second->mNode, muted ? 0.f : AudioValue( volume, 0, 20 ) );
        return true;
    }

    auto AudioMixer::RouteGroup( core::u64 id, core::u64 output ) -> bool {
        const auto it{ mImpl->mGroups.find( id ) };

        if ( !mIsInitialized || it == mImpl->mGroups.end() || ( output && !mImpl->mGroups.contains( output ) ) ) {
            return false;
        }

        auto cursor{ output };

        for ( size_t depth{}; cursor; ++depth ) {
            if ( cursor == id || depth >= mImpl->mGroups.size() ) {
                return false;
            }

            cursor = mImpl->mGroups.at( cursor )->mOutput;
        }

        if ( it->second->mOutput == output ) {
            return true;
        }

        ma_node* target{ output ? static_cast<ma_node*>( &mImpl->mGroups.at( output )->mNode ) : static_cast<ma_node*>( &mImpl->mMaster ) };

        if ( ma_node_attach_output_bus( &it->second->mNode, 0, target, 0 ) != MA_SUCCESS ) {
            return false;
        }

        it->second->mOutput = output;
        return true;
    }

    auto AudioMixer::RemoveGroup( core::u64 id ) -> void {
        if ( !mIsInitialized ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };
        const auto it{ mImpl->mGroups.find( id ) };

        if ( it == mImpl->mGroups.end() ) {
            return;
        }

        for ( auto& [other, group]: mImpl->mGroups ) {
            if ( group->mOutput == id ) {
                RouteGroup( other, 0 );
            }
        }

        for ( auto* source: mDevice->mImpl->mLiveSources ) {
            if ( source->mMixer == this && source->mMixerGroup == id ) {
                source->SetOutput( this );
            }
        }

        ma_sound_group_uninit( &it->second->mNode );
        mImpl->mGroups.erase( it );
    }

    auto AudioMixer::EndUpdate() -> void {
        for ( auto it{ mImpl->mGroups.begin() }; it != mImpl->mGroups.end(); ) {
            const auto id{ it->first };
            const bool used{ it->second->mUsed };
            ++it;

            if ( !used ) {
                RemoveGroup( id );
                it = mImpl->mGroups.begin();
            }
        }
    }

    auto AudioMixer::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };

        for ( auto* source: mDevice->mImpl->mLiveSources ) {
            if ( source->mMixer == this ) {
                source->SetOutput( nullptr );
            }
        }

        while ( !mImpl->mGroups.empty() ) {
            RemoveGroup( mImpl->mGroups.begin()->first );
        }

        ma_sound_group_uninit( &mImpl->mMaster );
        eastl::erase( mDevice->mImpl->mLiveMixers, this );
        mIsInitialized = false;
        mDevice = nullptr;
    }
}// namespace mikoto::audio
