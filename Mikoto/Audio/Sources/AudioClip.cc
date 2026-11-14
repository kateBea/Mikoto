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

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioBackend.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    Audio::Audio( filesystem::FileHandle file, core::f32 volume )
        : mFileSource{ eastl::move( file ) },
          mDefaultVolume{ AudioValue( volume, 0, 20 ) } {
    }

    Audio::~Audio() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Audio::CreateSource() const -> AudioSourceHandle {
        MKT_BEGIN_PROFILER();

        if ( !IsReady() || !mDevice || !mFileSource ) {
            return AudioSourceHandle{};
        }

        auto source = mDevice->CreateSource( mFileSource->GetPath() );

        if ( source ) {
            source->SetVolume( mDefaultVolume );
        }

        return source;
    }

    auto Audio::GetFile() const -> filesystem::FileHandle {
        MKT_BEGIN_PROFILER();
        return mFileSource;
    }

    auto Audio::GetTrackName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mFileSource ? mFileSource->GetName() : eastl::string_view{};
    }

    auto Audio::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated || !mDevice ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };

        if ( !mDevice->IsInitialized() || mFileSource.IsEmpty() ) {
            return;
        }

        // Registration and allocation state must agree so destruction cannot
        // leave an unallocated clip in the device's live-resource list.
        mDevice->mImpl->mLiveClips.push_back( this );
        mIsAllocated = true;
    }

    auto Audio::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mDevice ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };

        // Shutdown destroys from the back; preserve that O(1) removal path.
        auto& clips{ mDevice->mImpl->mLiveClips };

        if ( !clips.empty() && clips.back() == this ) {
            clips.pop_back();
        }
        else {
            eastl::erase( clips, this );
        }

        mIsAllocated = false;
        mDevice = nullptr;
    }
}// namespace mikoto::audio
