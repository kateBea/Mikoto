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

#include <Core/Exception.hh>

#include <Assets/AssetAudioClip.hh>

#include <Audio/AudioClip.hh>

namespace mikoto::asset {

    auto AudioLoadDescription::SetFile( filesystem::FileHandle source ) -> AudioLoadDescription & {
        MKT_BEGIN_PROFILER();
        mFile = source;
        mPath = mFile ? mFile->GetPath() : filesystem::Path{};
        return *this;
    }

    auto AudioLoadDescription::SetVolume( core::f32 volume ) -> AudioLoadDescription & {
        MKT_BEGIN_PROFILER();
        mVolume = volume;
        return *this;
    }


    AudioClip::AudioClip( core::Ref<audio::Audio> resource )
        : mResource{ eastl::move( resource ) } {
        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot construct AudioClip without a resource." );
        }
    }

    AudioClip::~AudioClip() = default;
    auto AudioClip::GetResource() const -> core::Ref<audio::Audio> {
        return mResource;
    }

    auto AudioClip::GetFile() const -> filesystem::FileHandle {
        return mResource->GetFile();
    }

    auto AudioClip::GetTrackName() const -> eastl::string_view {
        return mResource->GetTrackName();
    }

    auto AudioClip::CreateSource() const -> audio::AudioSourceHandle {
        return mResource->CreateSource();
    }

    auto AudioClip::CanEvict() const -> bool {
        return mResource->GetRefCount() == 1;
    }

    auto AudioClip::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        const auto file{ mResource->GetFile() };
        return { sizeof( *this ) + ( file ? file->GetSize() : 0 ), 0 };
    }
}// namespace mikoto::asset
