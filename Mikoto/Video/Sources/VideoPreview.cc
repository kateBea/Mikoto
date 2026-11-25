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

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include <EASTL/vector.h>

#include <Core/Exception.hh>
#include <Core/Platform.hh>
#include <Core/String.hh>

#include <Video/VideoPreview.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/Windows/Windows.hh>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#endif

namespace mikoto::video {

#if defined( MIKOTO_PLATFORM_WINDOWS )

    using Microsoft::WRL::ComPtr;

    static auto CheckPreviewResult( HRESULT result, core::cstr operation ) -> void {
        if ( FAILED( result ) ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "{} (HRESULT 0x{:08x})", operation, static_cast<core::u32>( result ) ) );
        }
    }

    class PreviewMediaSession final {
    public:
        explicit PreviewMediaSession() {
            const auto com{ CoInitializeEx( nullptr, COINIT_MULTITHREADED ) };

            // A caller already in an STA can still use a synchronous Source Reader.
            if ( com != RPC_E_CHANGED_MODE ) {
                CheckPreviewResult( com, "Initialize video preview COM" );
            }

            mOwnsCom = SUCCEEDED( com );
            const auto startup{ MFStartup( MF_VERSION, MFSTARTUP_LITE ) };

            if ( FAILED( startup ) ) {
                if ( mOwnsCom ) {
                    CoUninitialize();
                }

                CheckPreviewResult( startup, "Initialize video preview Media Foundation" );
            }
        }

        ~PreviewMediaSession() {
            MFShutdown();

            if ( mOwnsCom ) {
                CoUninitialize();
            }
        }

    private:
        bool mOwnsCom{};
    };

#endif

    auto DecodeVideoPreview( const filesystem::Path& path ) -> asset::ImageHandle {
#if defined( MIKOTO_PLATFORM_WINDOWS )
        const PreviewMediaSession session{};
        ComPtr<IMFAttributes> attributes{};
        CheckPreviewResult( MFCreateAttributes( &attributes, 2 ), "Create video preview attributes" );
        CheckPreviewResult( attributes->SetUINT32( MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE ), "Enable preview colour conversion" );
        CheckPreviewResult( attributes->SetUINT32( MF_SOURCE_READER_DISABLE_DXVA, TRUE ), "Select CPU preview decoding" );

        ComPtr<IMFSourceReader> reader{};
        const auto native{ path.GetPathTyped<std::filesystem::path>().wstring() };
        CheckPreviewResult( MFCreateSourceReaderFromURL( native.c_str(), attributes.Get(), &reader ), "Open video preview" );
        CheckPreviewResult( reader->SetStreamSelection( MF_SOURCE_READER_ALL_STREAMS, FALSE ), "Disable preview media streams" );
        CheckPreviewResult( reader->SetStreamSelection( MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE ), "Select preview video stream" );

        ComPtr<IMFMediaType> output{};
        CheckPreviewResult( MFCreateMediaType( &output ), "Create video preview format" );
        CheckPreviewResult( output->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video ), "Set preview media type" );
        CheckPreviewResult( output->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_RGB32 ), "Set preview RGB format" );
        CheckPreviewResult( reader->SetCurrentMediaType( MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output.Get() ), "Configure video preview decoder" );

        LONGLONG target{};
        PROPVARIANT duration{};

        if ( SUCCEEDED( reader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration ) ) && duration.vt == VT_UI8 ) {
            // Avoid a black opening frame without scanning the clip or decoding its audio.
            target = static_cast<LONGLONG>( std::min<ULONGLONG>( duration.uhVal.QuadPart / 10, 20000000 ) );
        }

        PropVariantClear( &duration );
        PROPVARIANT position{};
        position.vt = VT_I8;
        position.hVal.QuadPart = target;

        if ( target && FAILED( reader->SetCurrentPosition( GUID_NULL, position ) ) ) {
            target = 0;
        }

        ComPtr<IMFSample> sample{};

        for ( core::u32 attempt{}; attempt < 240; ++attempt ) {
            ComPtr<IMFSample> candidate{};
            DWORD flags{};
            LONGLONG timestamp{};
            CheckPreviewResult( reader->ReadSample( MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &timestamp, &candidate ), "Decode video preview frame" );

            if ( candidate ) {
                sample = candidate;

                if ( timestamp >= target ) {
                    break;
                }
            }

            if ( flags & MF_SOURCE_READERF_ENDOFSTREAM ) {
                break;
            }
        }

        if ( !sample ) {
            MKT_THROW_RUNTIME_ERROR( "The video has no decodable preview frame." );
        }

        output.Reset();
        CheckPreviewResult( reader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_VIDEO_STREAM, &output ), "Query video preview format" );
        UINT32 width{};
        UINT32 height{};
        CheckPreviewResult( MFGetAttributeSize( output.Get(), MF_MT_FRAME_SIZE, &width, &height ), "Query video preview dimensions" );

        if ( !width || !height || width > 8192 || height > 8192 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported video preview dimensions." );
        }

        eastl::vector<core::ubyte> pixels( core::usize{ width } * height * 4 );
        ComPtr<IMFMediaBuffer> buffer{};
        CheckPreviewResult( sample->ConvertToContiguousBuffer( &buffer ), "Access video preview pixels" );
        ComPtr<IMF2DBuffer> surface{};
        BYTE* scanline{};
        LONG stride{};
        bool locked2D{};

        if ( SUCCEEDED( buffer.As( &surface ) ) ) {
            CheckPreviewResult( surface->Lock2D( &scanline, &stride ), "Map video preview surface" );
            locked2D = true;
        }
        else {
            UINT32 packedStride{};

            if ( FAILED( output->GetUINT32( MF_MT_DEFAULT_STRIDE, &packedStride ) ) ) {
                CheckPreviewResult( MFGetStrideForBitmapInfoHeader( MFVideoFormat_RGB32.Data1, width, &stride ), "Query video preview stride" );
            }
            else {
                stride = static_cast<LONG>( packedStride );
            }

            DWORD bytes{};
            CheckPreviewResult( buffer->Lock( &scanline, nullptr, &bytes ), "Map video preview pixels" );

            if ( std::abs( static_cast<core::i64>( stride ) ) < width * 4 ||
                 core::u64{ height - 1 } * std::abs( static_cast<core::i64>( stride ) ) + width * 4 > bytes ) {
                buffer->Unlock();
                MKT_THROW_RUNTIME_ERROR( "Invalid video preview pixel stride." );
            }

            if ( stride < 0 ) {
                scanline += core::usize{ height - 1 } * static_cast<core::usize>( -stride );
            }
        }

        // Lock2D defines scanline zero as the displayed top row, even for bottom-up RGB buffers.
        // https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imf2dbuffer-lock2d
        try {
            if ( std::abs( static_cast<core::i64>( stride ) ) < width * 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid video preview surface stride." );
            }

            for ( core::u32 y{}; y < height; ++y ) {
                const auto* row{ scanline + static_cast<core::i64>( y ) * stride };

                for ( core::u32 x{}; x < width; ++x ) {
                    const core::usize destination{ ( core::usize{ y } * width + x ) * 4 };
                    pixels[destination] = row[x * 4 + 2];
                    pixels[destination + 1] = row[x * 4 + 1];
                    pixels[destination + 2] = row[x * 4];
                    pixels[destination + 3] = 255;
                }
            }
        } catch ( ... ) {
            if ( locked2D ) {
                surface->Unlock2D();
            }
            else {
                buffer->Unlock();
            }

            throw;
        }

        if ( locked2D ) {
            surface->Unlock2D();
        }
        else {
            buffer->Unlock();
        }

        auto image{ asset::ImageHandle::New() };
        image->mWidth = width;
        image->mHeight = height;
        image->mChannels = 4;
        image->mFormat = asset::ImageFormat::eRGBA8_UINT;
        image->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return image;
#else
        MKT_THROW_RUNTIME_ERROR( "Video previews require a platform frame decoder." );
#endif
    }

}// namespace mikoto::video
