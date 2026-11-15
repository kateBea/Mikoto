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

#include <mutex>
#include <stdexcept>

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Math/Bounds.hh>

#include <Threading/TaskService.hh>

#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>
#include <Renderer/ForwardRenderer/Passes/TonemapModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

#if MIKOTO_MODULE_AUDIO
#include <miniaudio.h>
#endif

namespace mikoto::renderer {

    // https://entropymine.com/imageworsener/resample/
    // https://entropymine.com/imageworsener/resizealpha/
    // https://entropymine.com/imageworsener/gamma/

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    struct ThumbnailRenderer::Preparation {
        std::mutex mMutex{};
        eastl::unique_ptr<ThumbnailPasses> mPasses{};
        asset::ModelHandle mModel{};
        ThumbnailPreviewSettings mSettings{};
        material::MaterialHandle mMaterial{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        eastl::string mError{};
        bool mReady{};
    };

    struct ThumbnailRenderer::PostProcessPreview {
        static constexpr auto kResolution{ rhi::RenderResolution::e720P };

        PostEffectsPass mEffects{ kResolution };
        TonemapModule mTonemap{ kResolution };

        eastl::unique_ptr<FrameGraph> mGraph{};
        rhi::TextureHandle mSource{};
        FGTextureHandle mOutput{};
    };

    auto ThumbnailRendererCreateInfo::SetName( eastl::string_view name ) -> ThumbnailRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetDevice( renderer::rhi::IGpuDevice *device ) -> ThumbnailRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mDevice = device;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetAssetCachePathThumbnails( eastl::string_view path ) -> ThumbnailRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mAssetCacheThumbnails = path;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetShaderBasePath( eastl::string_view path ) -> ThumbnailRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mShaderBasePath = path;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetRenderResolution( renderer::rhi::RenderResolution resolution ) -> ThumbnailRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mResolution = resolution;
        return *this;
    }

    ThumbnailRenderer::ThumbnailRenderer( const ThumbnailRendererCreateInfo &desc )
        : mDevice{ desc.mDevice }, mShaderPath{ desc.mShaderBasePath.empty() ? "Resources/Shaders/slang" : desc.mShaderBasePath }, mAssetCacheThumbnails{ desc.mAssetCacheThumbnails } {
    }

    ThumbnailRenderer::~ThumbnailRenderer() = default;

    auto ThumbnailRenderer::Initialize() -> void {
        // Compile shaders lazily when the first visible model requests a preview.
    }

    auto ThumbnailRenderer::Shutdown() -> void {
        // The owning editor waits for the device before destroying its panels
        // and renderers. A submitted graph must not be destroyed early.
        mPreparation.reset();
        mCallback = {};
        mGraph.reset();
        mPasses.reset();
        mPostProcessPreview.reset();
        mShowPostProcessPreview = false;
        mPreviewModel.Reset();
        mCompletedImage.Reset();
        mFlatPreview.Reset();
        mBusy = mCompleted = false;
    }

    auto ThumbnailRenderer::GenerateThumbnail( asset::ModelHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( width <= 0 || height <= 0 ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        if ( !BeginModelPreview( model,
                 ThumbnailPreviewSettings{},
                 material::MaterialHandle{},
                 {}, static_cast<core::u32>( width ), static_cast<core::u32>( height ) ) ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        return GetPreviewTexture();
    }

    auto ThumbnailRenderer::BeginModelPreview(
        asset::ModelHandle model,
        const ThumbnailPreviewSettings &settings,
        material::MaterialHandle material,
        PreviewCallback callback, core::u32 width, core::u32 height, asset::ImageHandle image ) -> bool {
        Poll();

        if ( mBusy || ( !model && !image ) ) {
            return false;
        }

        if ( !mDevice || ( !mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eVulkan ) && !mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D12 ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "GPU previews require the Vulkan or D3D12 frame graph backend." );
        }

        mError.clear();
        mShowPostProcessPreview = false;
        mCompletedImage.Reset();
        mCallback = eastl::move( callback );

        if ( image ) {
            // Upload caller-generated previews without lighting or post-processing.
            // Interactive edits must not invoke the project's import compressor.
            mFlatPreview = mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
                .SetWidth( width )
                .SetHeight( height )
                .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                .SetFormat( renderer::rhi::Format::eRGBA8_UNORM )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                .SetImageData( image ) );

            mCompletedImage = eastl::move( image );
            mCompleted = true;
            return true;
        }

        mFlatPreview.Reset();

        if ( !mGraph || mPreviewModel != model || width != mWidth || height != mHeight ) {
            auto pending{ eastl::make_shared<Preparation>() };
            pending->mModel = model;
            pending->mSettings = settings;
            pending->mMaterial = eastl::move( material );
            pending->mWidth = width;
            pending->mHeight = height;
            // The mailbox owns model lifetime even if this renderer is closed. Workers
            // build CPU bounds, rest palettes and materials; only Poll touches the graph.
            threading::TaskService::Get()->ExecuteWorkerTask( [pending] {
                eastl::unique_ptr<ThumbnailPasses> passes;
                eastl::string error;

                try {
                    RunImportJobs( 1, [&]( core::usize ) {
                        passes = eastl::make_unique<ThumbnailPasses>( pending->mModel, pending->mWidth, pending->mHeight );
                    } );
                } catch ( const std::exception &exception ) {
                    error = exception.what();
                }

                const std::lock_guard lock{ pending->mMutex };
                pending->mPasses = eastl::move( passes );
                pending->mError = eastl::move( error );
                pending->mReady = true;
            } );
            mPreparation = eastl::move( pending );
            mBusy = true;
        }
        else {
            SubmitPreview( settings, eastl::move( material ) );
        }

        return true;
    }

    auto ThumbnailRenderer::BeginPostProcessPreview( const rhi::TextureHandle& source,
        const material::PostProcessMaterial& material ) -> bool {
        Poll();

        if ( mBusy || !source ) {
            return false;
        }

        if ( !mDevice || ( !mDevice->IsGraphicsApi( rhi::GraphicsAPI::eVulkan ) && !mDevice->IsGraphicsApi( rhi::GraphicsAPI::eD3D12 ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "GPU previews require the Vulkan or D3D12 frame graph backend." );
        }

        if ( !mPostProcessPreview || mPostProcessPreview->mSource != source ) {
            auto preview{ eastl::make_unique<PostProcessPreview>() };
            preview->mSource = source;
            auto* shaders{ asset::AssetRegistryService::Get()->GetShaderLibrary() };
            preview->mGraph = FrameGraph::Create( mDevice, shaders );

            // Use the scene's stage order, with the HDR image in place of scene geometry.
            auto& graph{ *preview->mGraph };
            const auto input{ graph.ImportTexture( source ) };
            preview->mEffects.RegisterPostProcess( graph, material::PostProcessStage::eBeforeToneMapping, input );
            preview->mEffects.RegisterBloom( graph, graph.GetData<PostProcessModuleInfo>().mBeforeToneMapping.mColorImage );
            preview->mTonemap.RegisterTonemapPass( graph );
            preview->mEffects.RegisterPostProcess( graph, material::PostProcessStage::eAfterToneMapping,
                graph.GetData<GeomShadingModuleInfo>().mTonemapColor );

            preview->mOutput = graph.GetData<PostProcessModuleInfo>().mAfterToneMapping.mColorImage;
            graph.RegisterPass(
                "PostProcessPreview.SampledOutput",
                FGPassType::eGraphics,
                [output = preview->mOutput]( FGNodeBuilder& builder, core::Blackboard& ) {
                    // Leave the image ready for ImGui to sample outside the graph.
                    builder.UseResource( output, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                },
                []( CommandContext&, core::Blackboard& ) {
                } );
            graph.Compile();

            mPostProcessPreview = eastl::move( preview );
        }

        mError.clear();
        mPostProcessPreview->mEffects.SetPreviewMaterial( material );

        auto& graph{ *mPostProcessPreview->mGraph };
        graph.RegisterReadback( [this]( core::Blackboard&, const FGResourceManager& ) {
            mBusy = false;
        } );
        graph.Execute();

        mBusy = true;
        mShowPostProcessPreview = true;
        return true;
    }

    auto ThumbnailRenderer::SubmitPreview( const ThumbnailPreviewSettings &settings, material::MaterialHandle material ) -> void {
        const bool readback{ static_cast<bool>( mCallback ) };
        mPasses->Prepare( settings, eastl::move( material ), readback );
        mGraph->RegisterReadback( [this, readback]( core::Blackboard &, const FGResourceManager &resources ) {
            try {
                if ( readback ) {
                    mCompletedImage = mPasses->ReadImage( resources );
                }
            } catch ( const std::exception &error ) {
                mError = error.what();
            }

            mCompleted = true;
            mBusy = false;
        } );
        mGraph->Execute();
        mBusy = true;
    }

    auto ThumbnailRenderer::Poll() -> void {
        if ( mPreparation ) {
            auto pending{ mPreparation };
            // No worker ever holds this mutex while preparing a model.
            const std::lock_guard lock{ pending->mMutex };

            if ( pending->mReady ) {
                mPreparation.reset();

                try {
                    if ( !pending->mError.empty() ) {
                        MKT_THROW_RUNTIME_ERROR( pending->mError.c_str() );
                    }

                    // Publish completed CPU preparation before issuing work on the renderer thread.
                    mGraph.reset();
                    mPasses = eastl::move( pending->mPasses );
                    mShaderLibrary = asset::AssetRegistryService::Get()->GetShaderLibrary();
                    mGraph = FrameGraph::Create( mDevice, mShaderLibrary );

                    mPasses->RegisterUpload( *mGraph );
                    mPasses->RegisterShadows( *mGraph );
                    mPasses->RegisterPrefilter( *mGraph );
                    mPasses->RegisterEnvironment( *mGraph );
                    mPasses->RegisterMaterials( *mGraph );
                    mPasses->RegisterBloom( *mGraph );
                    mPasses->RegisterTonemap( *mGraph );
                    mPasses->RegisterReadback( *mGraph );
                    mPasses->RegisterSampledOutput( *mGraph );
                    mGraph->Compile();

                    mPreviewModel = pending->mModel;
                    mWidth = pending->mWidth;
                    mHeight = pending->mHeight;
                    SubmitPreview( pending->mSettings, eastl::move( pending->mMaterial ) );
                } catch ( const std::exception &error ) {
                    mError = error.what();
                    mBusy = false;
                    mCompleted = true;
                    mGraph.reset();
                }
            }
        }

        if ( mGraph ) {
            mGraph->ExecuteReadbacks();
        }

        if ( mPostProcessPreview ) {
            mPostProcessPreview->mGraph->ExecuteReadbacks();
        }

        if ( mCompleted ) {
            mCompleted = false;
            // Invoke clients after callback iteration, allowing them to enqueue
            // follow-up work without invalidating the readback manager iterator.
            auto callback{ eastl::move( mCallback ) };

            if ( callback ) {
                callback( eastl::move( mCompletedImage ) );
            }
        }
    }

    auto ThumbnailRenderer::IsBusy() const -> bool {
        return mBusy;
    }

    auto ThumbnailRenderer::GetPreviewTexture() const -> renderer::rhi::TextureHandle {
        if ( mShowPostProcessPreview ) {
            return mPostProcessPreview->mGraph->GetTexture( mPostProcessPreview->mOutput );
        }

        if ( mFlatPreview ) {
            return mFlatPreview;
        }

        return mGraph ? mGraph->GetTexture( mPasses->GetOutput() ) : renderer::rhi::TextureHandle{};
    }

    auto ThumbnailRenderer::GetPreviewError() const -> eastl::string_view {
        return mError;
    }

    auto ThumbnailRenderer::GenerateThumbnail( renderer::FontHandle font, core::i32 width, core::i32 height ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( font.IsEmpty() ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        return GenerateThumbnail( font->GetAtlas(), width, height );
    }

    auto ThumbnailRenderer::GenerateThumbnail( rhi::TextureHandle original, const rhi::TextureSlice &slice ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( original.IsEmpty() || slice.mMipLevel || slice.mArrayLayer || slice.z || slice.mDepth != 1 ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        const auto image{ original->GetImage() ? asset::DecompressImage( original->GetImage() ) : asset::ImageHandle{} };

        if ( image.IsEmpty() || image->mBufferSpan.IsEmpty() || !slice.mWidth || !slice.mHeight ||
             slice.x >= image->mWidth || slice.y >= image->mHeight ||
             slice.mWidth > image->mWidth - slice.x || slice.mHeight > image->mHeight - slice.y ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        const core::usize stride{ image->mFormat == asset::ImageFormat::eRGBA32_FLOAT ? 16u : 4u };

        if ( image->mBufferSpan->GetSize() < core::usize{ image->mWidth } * image->mHeight * stride ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        eastl::vector<core::ubyte> pixels( core::usize{ slice.mWidth } * slice.mHeight * stride );
        const auto *source{ static_cast<const core::ubyte *>( image->mBufferSpan->GetData() ) };

        for ( core::u32 y{}; y < slice.mHeight; ++y ) {
            eastl::copy_n( source + ( core::usize{ y + slice.y } * image->mWidth + slice.x ) * stride,
                slice.mWidth * stride, pixels.data() + core::usize{ y } * slice.mWidth * stride );
        }

        auto cropped{ asset::ImageHandle::New() };
        cropped->mWidth = slice.mWidth;
        cropped->mHeight = slice.mHeight;
        cropped->mChannels = 4;
        cropped->mFormat = image->mFormat;
        cropped->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return UploadPreview( RenderTexturePreview( cropped, 512, 512 ) );
    }

    auto ThumbnailRenderer::GenerateThumbnail( rhi::TextureHandle original, core::i32 width, core::i32 height ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( original.IsEmpty() || width <= 0 || height <= 0 ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        return UploadPreview( RenderTexturePreview( original->GetImage(), width, height ) );
    }

    auto ThumbnailRenderer::Create( const ThumbnailRendererCreateInfo &spec ) -> eastl::unique_ptr<ThumbnailRenderer> {
        MKT_BEGIN_PROFILER();
        return eastl::make_unique<ThumbnailRenderer>( spec );
    }

    auto ThumbnailRenderer::UploadPreview( asset::ImageHandle image ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( image.IsEmpty() || !mDevice ) {
            return rhi::TextureHandle::CreateEmpty();
        }

        image = asset::AssetRegistryService::Get()->IsTextureCompressionEnabled() ? CompressImage( image ) : DecompressImage( image );
        image = asset::PrepareImageForUpload( image, *mDevice );
        return mDevice->CreateTexture( rhi::TextureCreateDescription{}
            .SetWidth( image->mWidth )
            .SetHeight( image->mHeight )
            .SetDimensions( rhi::TextureDimension::eTexture2D )
            .SetFormat( asset::GetTextureFormat( image->mFormat ) )
            .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource )
            .SetImageData( image ) );
    }

    auto ThumbnailRenderer::RenderAudioPreview( const filesystem::Path &path, core::u32 width, core::u32 height ) -> asset::ImageHandle {
        MKT_BEGIN_PROFILER();

        if ( path.IsEmpty() || !width || !height || width > 4096 || height > 4096 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid audio thumbnail source or dimensions." );
        }

#if MIKOTO_MODULE_AUDIO
        // Each worker owns its decoder; thumbnail generation needs no playback device.
        ma_decoder decoder{};
        const ma_decoder_config config{ ma_decoder_config_init( ma_format_f32, 0, 0 ) };
        const auto native{ path.GetPathTyped<std::filesystem::path>() };
#if defined( MIKOTO_PLATFORM_WINDOWS )
        const ma_result initialized{ ma_decoder_init_file_w( native.c_str(), &config, &decoder ) };
#else
        const ma_result initialized{ ma_decoder_init_file( native.c_str(), &config, &decoder ) };
#endif
        if ( initialized != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot decode audio thumbnail '{}': {}", path.GetC_Str(), ma_result_description( initialized ) ) );
        }

        const eastl::unique_ptr<ma_decoder, decltype( &ma_decoder_uninit )> decoderOwner{ &decoder, ma_decoder_uninit };

        ma_uint32 channels{};

        if ( ma_decoder_get_data_format( &decoder, nullptr, &channels, nullptr, nullptr, 0 ) != MA_SUCCESS ||
             !channels || channels > MA_MAX_CHANNELS ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid audio thumbnail channel count." );
        }

        constexpr ma_uint64 framesPerBlock{ 4096 };
        eastl::vector<core::f32> samples( core::usize{ framesPerBlock } * channels );
        const auto readBlock{ [&]( ma_uint64 &frames ) {
            frames = 0;
            const ma_result result{ ma_decoder_read_pcm_frames( &decoder, samples.data(), framesPerBlock, &frames ) };

            if ( result != MA_SUCCESS && result != MA_AT_END ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot read audio thumbnail '{}': {}", path.GetC_Str(), ma_result_description( result ) ) );
            }

            return frames != 0;
        } };

        const auto rewind{ [&]() {
            if ( ma_decoder_seek_to_pcm_frame( &decoder, 0 ) != MA_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot rewind the audio thumbnail decoder." );
            }
        } };

        ma_uint64 frameCount{};
        const ma_result lengthResult{ ma_decoder_get_length_in_pcm_frames( &decoder, &frameCount ) };
        rewind();

        if ( lengthResult != MA_SUCCESS || !frameCount ) {
            // Some decoding backends cannot report a length. Count their frames
            // first so the preview still covers the complete recording.
            frameCount = 0;
            ma_uint64 frames{};

            while ( readBlock( frames ) ) {
                frameCount += frames;
            }

            rewind();
        }

        if ( !frameCount ) {
            MKT_THROW_RUNTIME_ERROR( "Audio thumbnail source contains no PCM frames." );
        }

        const core::u32 horizontalPadding{ width / 16 };
        const core::u32 verticalPadding{ height / 8 };
        const core::u32 plotWidth{ width - horizontalPadding * 2 };
        // Separate vertical bars keep the waveform readable at browser icon sizes.
        const core::u32 barWidth{ eastl::max( 1u, width / 256 ) };
        const core::u32 barCount{ eastl::max( 1u, plotWidth / ( barWidth * 2 ) ) };
        const core::u32 bins{ static_cast<core::u32>( eastl::min( frameCount, ma_uint64{ barCount } ) ) };
        eastl::vector<core::float2> peaks( bins, core::float2{} );
        const auto binEnd{ [&]( core::u32 bin ) {
            // Quotient/remainder arithmetic avoids overflowing frameCount * bins.
            return ( frameCount / bins ) * ( bin + 1 ) + ( frameCount % bins ) * ( bin + 1 ) / bins;
        } };

        core::u32 bin{};
        ma_uint64 cursor{};
        ma_uint64 frames{};

        while ( readBlock( frames ) ) {
            for ( ma_uint64 frame{}; frame < frames; ++frame, ++cursor ) {
                while ( bin + 1 < bins && cursor >= binEnd( bin ) ) {
                    ++bin;
                }

                for ( ma_uint32 channel{}; channel < channels; ++channel ) {
                    const core::f32 value{ samples[core::usize{ frame } * channels + channel] };
                    const core::f32 sample{ eastl::clamp( value, -1.0f, 1.0f ) };
                    peaks[bin].x = eastl::min( peaks[bin].x, sample );
                    peaks[bin].y = eastl::max( peaks[bin].y, sample );
                }
            }
        }

        if ( !cursor ) {
            MKT_THROW_RUNTIME_ERROR( "Audio thumbnail decoder returned no PCM frames." );
        }

        core::f32 peak{};

        for ( const auto &range: peaks ) {
            peak = eastl::max( peak, eastl::max( -range.x, range.y ) );
        }

        const core::u32 center{ height / 2 };
        const core::f32 amplitude{ core::f32( eastl::min( center - verticalPadding, height - 1 - verticalPadding - center ) ) };
        const core::f32 scale{ peak > 1e-8f ? amplitude / peak : 0.0f };
        eastl::vector<core::ubyte> pixels( core::usize{ width } * height * 4, 0 );

        for ( core::usize pixel{}; pixel < core::usize{ width } * height; ++pixel ) {
            pixels[pixel * 4 + 3] = 255;
        }

        for ( core::u32 bar{}; bar < barCount; ++bar ) {
            const core::u32 x{ barCount > 1 ? bar * ( plotWidth - barWidth ) / ( barCount - 1 ) : ( plotWidth - barWidth ) / 2 };
            const core::float2 range{ peaks[eastl::min( bar * bins / barCount, bins - 1 )] };
            const core::f32 top{ center - range.y * scale };
            const core::f32 bottom{ center - range.x * scale };
            const core::u32 firstRow{ static_cast<core::u32>( eastl::max( 0.0f, glm::floor( top ) ) ) };
            const core::u32 lastRow{ static_cast<core::u32>( eastl::min( core::f32( height - 1 ), glm::ceil( bottom ) ) ) };

            for ( core::u32 y{ firstRow }; y <= lastRow; ++y ) {
                const core::f32 coverage{ eastl::clamp( eastl::min( core::f32( y ) + 0.5f, bottom + 0.5f ) -
                                                            eastl::max( core::f32( y ) - 0.5f, top - 0.5f ),
                    0.0f, 1.0f ) };
                const core::ubyte white{ static_cast<core::ubyte>( glm::round( coverage * 255.0f ) ) };

                for ( core::u32 column{}; column < barWidth; ++column ) {
                    const core::usize destination{ ( core::usize{ y } * width + horizontalPadding + x + column ) * 4 };
                    pixels[destination] = pixels[destination + 1] = pixels[destination + 2] = white;
                }
            }
        }

        auto image{ asset::ImageHandle::New() };
        image->mWidth = width;
        image->mHeight = height;
        image->mChannels = 4;
        image->mFormat = asset::ImageFormat::eRGBA8_UINT;
        image->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return image;
#else
        MKT_THROW_RUNTIME_ERROR( "Audio thumbnails require the Audio module." );
#endif
    }

    auto ThumbnailRenderer::RenderTexturePreview( const asset::ImageHandle &encoded, core::u32 width, core::u32 height ) -> asset::ImageHandle {
        MKT_BEGIN_PROFILER();
        const auto source{ asset::DecompressImage( encoded ) };

        if ( source.IsEmpty() || source->mBufferSpan.IsEmpty() || !source->mWidth || !source->mHeight ||
             !width || !height || width > 4096 || height > 4096 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid thumbnail image dimensions." );
        }

        const bool hdr{ source->mFormat == asset::ImageFormat::eRGBA32_FLOAT };
        const core::usize required{ core::usize{ source->mWidth } * source->mHeight * 4 * ( hdr ? MKT_SIZEOF( core::f32 ) : MKT_SIZEOF( core::ubyte ) ) };

        if ( source->mChannels != 4 || source->mBufferSpan->GetSize() < required ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid thumbnail pixel buffer." );
        }

        const core::f32 scale{ glm::min( core::f32( width ) / source->mWidth, core::f32( height ) / source->mHeight ) };
        const core::u32 fittedWidth{ glm::clamp( core::u32( glm::round( source->mWidth * scale ) ), 1u, width ) };
        const core::u32 fittedHeight{ glm::clamp( core::u32( glm::round( source->mHeight * scale ) ), 1u, height ) };
        const core::u32 left{ ( width - fittedWidth ) / 2 };
        const core::u32 top{ ( height - fittedHeight ) / 2 };
        eastl::vector<core::ubyte> pixels( core::usize{ width } * height * 4, 0 );

        const auto sample = [&]( core::u32 x, core::u32 y ) {
            const core::usize index{ ( core::usize{ y } * source->mWidth + x ) * 4 };
            core::float4 color{};

            if ( hdr ) {
                const auto *data{ static_cast<const core::f32 *>( source->mBufferSpan->GetData() ) };
                color = { data[index], data[index + 1], data[index + 2], data[index + 3] };
            }
            else {
                const auto *data{ static_cast<const core::ubyte *>( source->mBufferSpan->GetData() ) };
                color = core::float4{ data[index], data[index + 1], data[index + 2], data[index + 3] } / 255.0f;

                for ( core::u32 channel{}; channel < 3; ++channel ) {
                    color[channel] = glm::pow( color[channel], 2.2f );
                }
            }

            color.a = eastl::clamp( color.a, 0.0f, 1.0f );
            return core::float4{ core::float3{ color } * color.a, color.a };
        };

        // Area averaging suppresses aliasing when shrinking large textures.
        // Filtering premultiplied linear color prevents dark transparent edges.
        // Transparent padding fits the original aspect inside a 512-square preview.
        for ( core::u32 y{}; y < fittedHeight; ++y ) {
            const core::f32 y0{ core::f32( y ) * source->mHeight / fittedHeight };
            const core::f32 y1{ core::f32( y + 1 ) * source->mHeight / fittedHeight };

            for ( core::u32 x{}; x < fittedWidth; ++x ) {
                const core::f32 x0{ core::f32( x ) * source->mWidth / fittedWidth };
                const core::f32 x1{ core::f32( x + 1 ) * source->mWidth / fittedWidth };
                core::float4 color{};
                core::f32 weightSum{};

                for ( core::u32 sy{ core::u32( y0 ) }; sy < eastl::min( source->mHeight, core::u32( glm::ceil( y1 ) ) ); ++sy ) {
                    const core::f32 wy{ eastl::min( y1, core::f32( sy + 1 ) ) - eastl::max( y0, core::f32( sy ) ) };

                    for ( core::u32 sx{ core::u32( x0 ) }; sx < eastl::min( source->mWidth, core::u32( glm::ceil( x1 ) ) ); ++sx ) {
                        const core::f32 weight{ wy * ( eastl::min( x1, core::f32( sx + 1 ) ) - eastl::max( x0, core::f32( sx ) ) ) };
                        color += sample( sx, sy ) * weight;
                        weightSum += weight;
                    }
                }

                color /= eastl::max( weightSum, 1e-8f );
                const core::usize destination{ ( core::usize{ top + y } * width + left + x ) * 4 };

                for ( core::u32 channel{}; channel < 3; ++channel ) {
                    core::f32 value{ color.a > 1e-8f ? color[channel] / color.a : 0.0f };
                    value = eastl::max( value, 0.0f );

                    if ( hdr ) {
                        value /= 1.0f + value;
                    }

                    pixels[destination + channel] = core::ubyte( glm::round( glm::pow( eastl::clamp( value, 0.0f, 1.0f ), 1.0f / 2.2f ) * 255.0f ) );
                }

                pixels[destination + 3] = core::ubyte( glm::round( color.a * 255.0f ) );
            }
        }

        auto image{ asset::ImageHandle::New() };

        image->mWidth = width;
        image->mHeight = height;
        image->mChannels = 4;
        image->mFormat = asset::ImageFormat::eRGBA8_UINT;
        image->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return image;
    }
}// namespace mikoto::renderer
