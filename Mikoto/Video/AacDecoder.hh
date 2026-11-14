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

#ifndef MIKOTO_VIDEO_AAC_DECODER_HH
#define MIKOTO_VIDEO_AAC_DECODER_HH

#include <Core/Platform.hh>

#include <Video/VideoAudioStream.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/Windows/Windows.hh>

#include <mftransform.h>
#include <wrl/client.h>
#endif

namespace mikoto::video {

    class AacDecoder final : public core::INonCopyable {
    public:
        explicit AacDecoder( const AudioTrackInfo& track );
        auto Decode( const EncodedPacket& packet ) -> eastl::vector<PcmBlock>;
        auto Drain() -> eastl::vector<PcmBlock>;

    private:
        auto ReadOutput() -> eastl::vector<PcmBlock>;

        AudioTrackInfo mTrack{};
        core::i64 mNextOutputTime{};

#if defined( MIKOTO_PLATFORM_WINDOWS )
        Microsoft::WRL::ComPtr<IMFTransform> mDecoder{};
        core::u32 mOutputBufferBytes{};
#endif
    };
}

#endif
