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
#include <chrono>
#include <filesystem>

#include <EASTL/sort.h>
#include <EASTL/deque.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/File.hh>

#include <Assets/Asset.hh>
#include <Assets/Utility.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/ImageProcessing.hh>

#include <Material/Utility.hh>

#include <Renderer/Particle/ParticleEmitter.hh>

#include <Application/ThumbnailCache.hh>

namespace mikoto::editor {

    using namespace core;
    using namespace asset;
    using namespace filesystem;
    using namespace renderer;


    static constexpr core::u32 kSampleSize{ 512 };

    static auto CreateTextCoverage() -> eastl::vector<core::ubyte> {
        filesystem::Path path{ "Resources/Fonts/Inter/static/Inter-Regular.ttf" };

        if ( !path.Exists() ) {
            path = filesystem::Path{ "Mikoto/Resources/Resources/Fonts/Inter/static/Inter-Regular.ttf" };
        }

        FT_Library library{};

        if ( FT_Init_FreeType( &library ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot initialize the material preview font." );
        }

        const eastl::unique_ptr<FT_LibraryRec_, decltype( &FT_Done_FreeType )> libraryOwner{ library, FT_Done_FreeType };
        FT_Face face{};

        if ( FT_New_Face( library, path.GetC_Str(), 0, &face ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot load the material preview font." );
        }

        const eastl::unique_ptr<FT_FaceRec_, decltype( &FT_Done_Face )> faceOwner{ face, FT_Done_Face };
        eastl::vector<core::ubyte> coverage( kSampleSize * kSampleSize, 0 );
        const auto line{ [&]( core::cstr text, core::u32 size, core::i32 baseline ) {
            if ( FT_Set_Pixel_Sizes( face, 0, size ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot size the material preview font." );
            }

            // Measure before rasterizing so each sample line is centered.
            core::i32 advance{};

            for ( auto* character{ text }; *character; ++character ) {
                if ( FT_Load_Char( face, *character, FT_LOAD_RENDER ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Cannot rasterize the material preview text." );
                }

                advance += static_cast<core::i32>( face->glyph->advance.x >> 6 );
            }

            core::i32 pen{ ( static_cast<core::i32>( kSampleSize ) - advance ) / 2 };

            for ( auto* character{ text }; *character; ++character ) {
                if ( FT_Load_Char( face, *character, FT_LOAD_RENDER ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Cannot rasterize the material preview text." );
                }

                const auto& glyph{ *face->glyph };
                const auto& bitmap{ glyph.bitmap };

                for ( core::u32 row{}; row < bitmap.rows; ++row ) {
                    const core::i32 y{ baseline - glyph.bitmap_top + static_cast<core::i32>( row ) };
                    const auto* pixels{ bitmap.buffer + ( bitmap.pitch >= 0 ? row : bitmap.rows - 1 - row ) * static_cast<core::u32>( glm::abs( bitmap.pitch ) ) };

                    for ( core::u32 column{}; column < bitmap.width; ++column ) {
                        const core::i32 x{ pen + glyph.bitmap_left + static_cast<core::i32>( column ) };

                        if ( x >= 0 && y >= 0 && x < kSampleSize && y < kSampleSize ) {
                            auto& destination{ coverage[y * kSampleSize + x] };
                            destination = eastl::max( destination, pixels[column] );
                        }
                    }
                }

                pen += static_cast<core::i32>( glyph.advance.x >> 6 );
            }
        } };
        line( "Aa", 176, 280 );
        line( "Mikoto", 48, 356 );
        return coverage;
    }

    struct LoadedPreview {
        asset::ImageHandle mImage{};
        asset::ModelHandle mModel{};
        filesystem::Path mCachePath{};
        filesystem::Path mMaterialPath{};
        asset::AssetHandle<asset::Model> mRequest{};
    };

    static auto SavePreview( const asset::ImageHandle& image, const filesystem::Path& cached, bool compress ) -> asset::ImageHandle {
        auto encoded{ compress ? asset::CompressImage( image ) : image };

        try {
            asset::WriteImageCompressed( cached, encoded );
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_WARN( "Cannot cache thumbnail '{}': {}", cached.GetC_Str(), error.what() );
        }

        return encoded;
    }

    static auto LoadPreview( const filesystem::Path& source, const filesystem::Path& directory, bool compress, asset::ModelHandle sphere = asset::ModelHandle::CreateEmpty() ) -> LoadedPreview {
        MKT_BEGIN_PROFILER();
        const auto native{ source.GetPathTyped<std::filesystem::path>() };
        // Version and source metadata invalidate stale files without loading the
        // original texture again. The stable hash survives editor restarts.
        const auto identity{ std::filesystem::absolute( native ).generic_string() };
        core::u64 hash{ 14695981039346656037ull };

        for ( const auto character: identity ) {
            hash = ( hash ^ static_cast<core::ubyte>( character ) ) * 1099511628211ull;
        }

        hash ^= static_cast<core::u64>( std::filesystem::last_write_time( native ).time_since_epoch().count() );
        hash ^= std::filesystem::file_size( native );
        const bool isModel{ asset::IsFileModel( source ) };
        const bool isMaterial{ native.extension() == ".mktmtl" };
        const bool isParticle{ native.extension() == ".mktpart" };
        const bool isAudio{ asset::IsFileAudio( source ) };
        // Regenerate material spheres and models with the tighter camera framing.
        const core::u32 version{ isMaterial ? 8u : isModel ? 7u : isParticle ? 3u : 2u };
        const filesystem::Path cached{ directory.GetPathTyped<std::filesystem::path>() / string::Format( "{}-{}-v{}-{:016x}.ktx2", isAudio ? "audio" : ( isParticle ? "particle" : ( isMaterial ? "material" : ( isModel ? "gpu" : "texture" ) ) ), compress ? "bc7" : "rgba8", version, hash ).c_str() };

        if ( cached.Exists() ) {
            try {
                auto image{ asset::ProcessImageCompressed( filesystem::FileHandle::New( cached ) ) };

                if ( !image.IsEmpty() && image->mWidth == 512 && image->mHeight == 512 && !image->mBufferSpan.IsEmpty() ) {
                    return { image, asset::ModelHandle::CreateEmpty(), cached };
                }
            } catch ( const std::exception& ) {
                // An interrupted or corrupt cache file can always be rebuilt.
            }
        }

        if ( isAudio ) {
            auto image{ renderer::ThumbnailRenderer::RenderAudioPreview( source ) };
            image = SavePreview( image, cached, compress );
            return { image, asset::ModelHandle{}, cached };
        }

        if ( isParticle ) {
            const auto effect{ asset::AssetRegistryService::Get()->LoadAsset<asset::ParticleEffect>( source ) };

            if ( !effect ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot load particle effect thumbnail source." );
            }

            auto image{ RenderParticlePreview( *effect.GetPtr() ) };
            image = SavePreview( image, cached, compress );
            return { image, asset::ModelHandle{}, cached };
        }

        if ( isMaterial ) {
            const filesystem::File file{ source };
            const auto document{ YAML::Load( file.GetContentsString().c_str() ) };
            const auto type{ document["Type"].as<std::string>( "" ) };

            if ( type == "TextMaterial" ) {
                // This type has no GPU texture dependencies. Render its sample
                // on the worker without importing a sphere or HDR environment.
                const auto material{ material::LoadMaterial( filesystem::FileHandle::New( source ) ) };
                const auto* text{ dynamic_cast<const material::TextMaterial*>( material.GetPtr() ) };
                auto image{ RenderTextMaterialPreview( *text ) };
                image = SavePreview( image, cached, compress );
                return { image, asset::ModelHandle{}, cached };
            }

            // These domains have no sphere preview. Keep the browser's file icon.
            if ( type == "VolumeMaterial" || type == "ParticleMaterial" ) {
                return {};
            }

            if ( !sphere ) {
                return { {}, {}, cached, source, asset::AssetRegistryService::Get()->Request<asset::Model>( asset::AssetRef<asset::Model>{ MaterialPreviewSpherePath() }, asset::ModelLoadDescription{ .mPriority = LoadPriority::eBackground } ) };
            }

            return { asset::ImageHandle{}, sphere, cached, source };
        }

        if ( isModel ) {
            // Share residency with scene imports. The preview owns a demand,
            // not a second import, and drops it when cancelled or rendered.
            return { {}, {}, cached, {}, asset::AssetRegistryService::Get()->Request<asset::Model>( asset::AssetRef<asset::Model>{ source }, asset::ModelLoadDescription{ .mPriority = LoadPriority::eBackground } ) };
        }

        auto image{ renderer::ThumbnailRenderer::RenderTexturePreview( LoadTextureImage( source, asset::AssetRegistryService::Get()->GetAssetCacheBasePath(), compress ), 512, 512 ) };
        image = SavePreview( image, cached, compress );
        return { image, asset::ModelHandle::CreateEmpty(), cached };
    }


    struct ThumbnailCache::CacheState {
        struct Entry {
            Thumbnail mThumbnail{};
            asset::ImageHandle mReady{};
            asset::ModelHandle mReadyModel{};
            asset::AssetHandle<asset::Model> mRequest{};
            eastl::vector<asset::AssetHandle<asset::Texture>> mTextureRequests{};
            bool mAwaitingModel{};
            std::chrono::steady_clock::time_point mLastRequested{ std::chrono::steady_clock::now() };
            filesystem::Path mCachePath{};
            filesystem::Path mMaterialPath{};
            std::filesystem::file_time_type mModified{};
            std::chrono::steady_clock::time_point mChecked{ std::chrono::steady_clock::now() };
            std::chrono::steady_clock::time_point mRetryAfter{};
            bool mFailed{};
            core::u64 mGeneration{};
        };

        std::mutex mMutex{};
        filesystem::Path mDirectory{};
        bool mCompressTextures{ asset::AssetRegistryService::Get()->IsTextureCompressionEnabled() };
        renderer::rhi::TextureHandle mEnvironment{};
        bool mEnvironmentAttempted{};
        asset::ModelHandle mMaterialSphere{};
        ankerl::unordered_dense::map<filesystem::Path, Entry> mEntries{};
        eastl::deque<filesystem::Path> mUploadQueue{};
        eastl::deque<filesystem::Path> mModelQueue{};
        core::u32 mActiveJobs{};
        core::u32 mPendingModels{};
        core::u32 mGpuJobs{};
        core::u64 mNextGeneration{ 1 };
    };

    auto MaterialPreviewSpherePath() -> filesystem::Path {
        filesystem::Path path{ "Resources/Prefabs/material_sphere/scene.gltf" };

        if ( !path.Exists() ) {
            path = filesystem::Path{ "Mikoto/Resources/Resources/Prefabs/material_sphere/scene.gltf" };
        }

        if ( !path.Exists() ) {
            MKT_THROW_RUNTIME_ERROR( "Material preview sphere was not found." );
        }

        return path;
    }

    auto RenderTextMaterialPreview( const material::TextMaterial& material, core::u32 width, core::u32 height ) -> asset::ImageHandle {
        if ( !width || !height || width > 4096 || height > 4096 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid text material preview dimensions." );
        }

        // Coverage is immutable and shared; editing a color never rebuilds a font
        // atlas or creates a scene. Local FreeType state also permits worker use.
        static const auto coverage{ CreateTextCoverage() };
        const auto color{ material.Evaluate( core::float4{ 1.0f } ) };
        eastl::vector<core::ubyte> pixels( core::usize{ width } * height * 4 );

        for ( core::u32 y{}; y < height; ++y ) {
            const core::u32 sy{ y * kSampleSize / height };

            for ( core::u32 x{}; x < width; ++x ) {
                const core::u32 sx{ x * kSampleSize / width };
                const core::f32 background{ ( ( sx / 32 + sy / 32 ) & 1 ) ? 0.22f : 0.32f };
                const core::f32 alpha{ coverage[sy * kSampleSize + sx] / 255.0f * glm::clamp( color.a, 0.0f, 1.0f ) };
                const core::usize index{ ( core::usize{ y } * width + x ) * 4 };

                for ( core::u32 channel{}; channel < 3; ++channel ) {
                    pixels[index + channel] = static_cast<core::ubyte>( glm::round( glm::mix( background, glm::clamp( color[channel], 0.0f, 1.0f ), alpha ) * 255.0f ) );
                }

                pixels[index + 3] = 255;
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

    auto RenderParticlePreview( const asset::ParticleEffect& effect ) -> asset::ImageHandle {
        MKT_BEGIN_PROFILER();

        constexpr core::i32 size{ 512 };
        eastl::vector<ParticleEmitterInstance> instances{};
        instances.reserve( effect.GetEmitters().size() );

        for ( const auto& emitter: effect.GetEmitters() ) {
            instances.emplace_back( emitter );
        }

        eastl::vector<Particle> snapshot{};
        float best{};

        // Pick a visible sample over a bounded warmup so one-shot bursts
        // contribute even when they have already drained at the final tick.
        for ( core::i32 frame{}; frame < 180; ++frame ) {
            float score{};
            core::u64 seed{ 1 };

            for ( auto& instance: instances ) {
                instance.Update( 1.0f / 60.0f, core::float4x4{ 1.0f }, seed++ );
                for ( const auto& particle: instance.GetParticles() ) {
                    const auto age{ glm::clamp( particle.mAge / particle.mLifetime, 0.0f, 1.0f ) };
                    score += particle.GetColor().a * ( 0.2f + age * ( 1 - age ) );
                }
            }

            if ( score > best ) {
                best = score;
                snapshot.clear();

                for ( const auto& instance: instances ) {
                    const auto& particles{ instance.GetParticles() };
                    const auto count{ eastl::min<core::usize>( particles.size(), kMaxRenderedParticles - snapshot.size() ) };
                    snapshot.insert( snapshot.end(), particles.begin(), particles.begin() + count );
                }
            }
        }

        // Project the snapshot into a fixed thumbnail view and collect its bounds.
        struct Disc {
            float x;
            float y;
            float z;

            float r;
            float softness;
            core::float4 color;
        };
        eastl::vector<Disc> discs;
        const core::float3 right{ .819152f, 0, -.573576f };
        const core::float3 up{ -.177245f, .951057f, -.253132f };
        const core::float3 forward{ glm::cross( right, up ) };
        core::float2 low{ 0 };
        core::float2 high{ 0 };

        for ( const auto& p: snapshot ) {
            const float age{ glm::clamp( p.mAge / p.mLifetime, 0.0f, 1.0f ) };
            const float radius{ glm::mix( p.mStartSize, p.mEndSize, age ) * .5f };
            const auto color{ p.GetColor() };

            if ( radius <= 0 || color.a <= .001f ) {
                continue;
            }

            Disc d{ glm::dot( p.mPosition, right ), glm::dot( p.mPosition, up ), glm::dot( p.mPosition, forward ), radius, p.mSoftness, color };
            low = glm::min( low, core::float2{ d.x - radius, d.y - radius } );
            high = glm::max( high, core::float2{ d.x + radius, d.y + radius } );
            discs.push_back( d );
        }

        // Composite soft particles back to front, matching their runtime coverage.
        eastl::sort( discs.begin(), discs.end(), []( const auto& a, const auto& b ) {
            return a.z < b.z;
        } );
        const core::float2 center{ ( low + high ) * .5f };
        const float scale{ 440.0f / glm::max( glm::max( high.x - low.x, high.y - low.y ), .01f ) };
        eastl::vector<core::float3> pixels( size * size, core::float3{ .018f, .023f, .035f } );

        for ( const auto& d: discs ) {
            const float x{ 256 + ( d.x - center.x ) * scale };
            const float y{ 256 - ( d.y - center.y ) * scale };
            const float r{ glm::max( d.r * scale, .5f ) };
            const int x0{ glm::clamp( int( glm::floor( x - r ) ), 0, size - 1 ) };
            const int x1{ glm::clamp( int( glm::ceil( x + r ) ), 0, size - 1 ) };
            const int y0{ glm::clamp( int( glm::floor( y - r ) ), 0, size - 1 ) };
            const int y1{ glm::clamp( int( glm::ceil( y + r ) ), 0, size - 1 ) };
            const float feather{ glm::max( d.softness, 1.0f / r ) };

            for ( int py = y0; py <= y1; ++py ) {
                for ( int px = x0; px <= x1; ++px ) {
                    const float radius{ glm::length( core::float2{ px + .5f - x, py + .5f - y } ) / r };
                    const float coverage{ 1 - glm::smoothstep( glm::max( 0.0f, 1 - feather ), 1.0f, radius ) };
                    const float alpha{ glm::clamp( d.color.a * coverage, 0.0f, 1.0f ) };
                    pixels[py * size + px] = glm::mix( pixels[py * size + px], core::float3{ d.color }, alpha );
                }
            }
        }

        // Encode the display image into the thumbnail cache's RGBA format.
        eastl::vector<core::ubyte> bytes( size * size * 4 );

        for ( int i = 0; i < size * size; ++i ) {
            for ( int c = 0; c < 3; ++c ) {
                bytes[i * 4 + c] = static_cast<core::ubyte>( glm::clamp( glm::pow( glm::max( pixels[i][c], 0.0f ), 1.0f / 2.2f ), 0.0f, 1.0f ) * 255 + .5f );
            }

            bytes[i * 4 + 3] = 255;
        }

        auto image{ asset::ImageHandle::New() };
        image->mWidth = size;
        image->mHeight = size;
        image->mChannels = 4;
        image->mFormat = asset::ImageFormat::eRGBA8_UINT;
        image->mBufferSpan = memory::BufferSpanHandle::New( bytes.data(), bytes.size() );
        return image;
    }


    ThumbnailCache::ThumbnailCache( renderer::ThumbnailRenderer* renderer, renderer::rhi::IGpuDevice* device, eastl::string_view directory )
        : mState{ eastl::make_shared<CacheState>() }, mDevice{ device }, mThumbnailRenderer{ renderer } {
        MKT_BEGIN_PROFILER();
        SetDirectory( filesystem::Path{ directory } );
    }

    auto ThumbnailCache::SetDirectory( const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mState->mMutex };
        mState->mDirectory = path;
    }

    auto ThumbnailCache::Contains( const filesystem::Path& path ) const -> bool {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mState->mMutex };
        const auto found{ mState->mEntries.find( path ) };

        if ( found == mState->mEntries.end() ) {
            return false;
        }

        const auto now{ std::chrono::steady_clock::now() };
        found->second.mLastRequested = now;
        // Keep failures briefly to avoid resubmitting a broken file every frame,
        // then allow a fresh request even if its modification time is unchanged.
        if ( found->second.mFailed && now >= found->second.mRetryAfter ) {
            mState->mPendingModels -= found->second.mAwaitingModel ? 1 : 0;
            mState->mEntries.erase( found );
            return false;
        }

        if ( now - found->second.mChecked > std::chrono::seconds{ 1 } ) {
            found->second.mChecked = now;
            std::error_code error{};
            const auto modified{ std::filesystem::last_write_time( path.GetPathTyped<std::filesystem::path>(), error ) };

            if ( !error && modified != found->second.mModified ) {
                mState->mPendingModels -= found->second.mAwaitingModel ? 1 : 0;
                mState->mEntries.erase( found );
                return false;
            }
        }

        return true;
    }

    auto ThumbnailCache::GetThumbnail( const filesystem::Path& path ) const -> Thumbnail {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mState->mMutex };
        const auto found{ mState->mEntries.find( path ) };

        if ( found != mState->mEntries.end() ) {
            return found->second.mThumbnail;
        }

        return Thumbnail{};
    }

    auto ThumbnailCache::CreateThumbnail( const filesystem::Path& path ) -> Thumbnail {
        MKT_BEGIN_PROFILER();

        if ( Contains( path ) ) {
            return GetThumbnail( path );
        }

        if ( asset::IsFileAudio( path ) || asset::IsFileModel( path ) || path.GetPathTyped<std::filesystem::path>().extension() == ".mktmtl" ) {
            // Audio decoding stays on workers. GPU model submissions are batched
            // at frame end, so waiting here would deadlock their submission.
            CreateThumbnailAsync( path );
            return {};
        }

        filesystem::Path directory;
        {
            std::lock_guard lock{ mState->mMutex };
            directory = mState->mDirectory;
        }

        Thumbnail result{ mThumbnailRenderer->UploadPreview( LoadPreview( path, directory, asset::AssetRegistryService::Get()->IsTextureCompressionEnabled() ).mImage ) };
        std::lock_guard lock{ mState->mMutex };

        mState->mEntries[path].mThumbnail = result;

        std::error_code error{};
        mState->mEntries[path].mModified = std::filesystem::last_write_time( path.GetPathTyped<std::filesystem::path>(), error );
        return result;
    }

    auto ThumbnailCache::CreateThumbnailAsync( const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();

        filesystem::Path directory{};
        core::u64 generation{};
        asset::ModelHandle sphere{};
        const auto state{ mState };
        {
            std::lock_guard lock{ state->mMutex };
            // Only visible entries request previews. Leave excess requests
            // uncached so the browser retries next frame without filling all
            // workers (and memory) with simultaneous model imports.
            if ( state->mActiveJobs + state->mPendingModels + state->mGpuJobs + state->mModelQueue.size() + state->mUploadQueue.size() >= 2 ) {
                return;
            }

            if ( !state->mEntries.try_emplace( path ).second ) {
                return;
            }

            directory = state->mDirectory;
            sphere = state->mMaterialSphere;
            auto& entry{ state->mEntries.at( path ) };
            generation = entry.mGeneration = state->mNextGeneration++;

            std::error_code error{};
            entry.mModified = std::filesystem::last_write_time( path.GetPathTyped<std::filesystem::path>(), error );

            ++state->mActiveJobs;
        }

        // Imports use the same worker-safe factory as asynchronous scene loads.
        // Graph recording and preview rendering stay on the editor/render thread.
        try {
            threading::TaskService::Get()->ExecuteWorkerTask(
                [state, path, directory, generation, sphere, compress = state->mCompressTextures]() {
                try {
                    auto loaded{ LoadPreview( path, directory, compress, sphere ) };
                    bool loadEnvironment{};

                    if ( loaded.mModel || loaded.mRequest.GetState() != asset::AssetState::eUnloaded ) {
                        const std::lock_guard lock{ state->mMutex };
                        loadEnvironment = !state->mEnvironmentAttempted;
                        state->mEnvironmentAttempted = true;
                    }

                    if ( loadEnvironment ) {
                        // HDR decoding/compression and its upload wait must never run in UploadReady.
                        filesystem::Path environment{ "Resources/Cubemaps/modern_evening_street_4k.hdr" };

                        try {
                            auto texture{ environment.Exists() ? asset::AssetRegistryService::Get()->LoadAsset<rhi::ITexture>( environment, rhi::TextureDimension::eTexture2D ) : rhi::TextureHandle{} };

                            const std::lock_guard lock{ state->mMutex };
                            state->mEnvironment = eastl::move( texture );
                        } catch ( const std::exception& error ) {
                            MKT_CORE_LOGGER_WARN( "Thumbnail HDR environment: {}. Using studio lighting.", error.what() );
                        }
                    }

                    std::lock_guard lock{ state->mMutex };

                    if ( !loaded.mMaterialPath.IsEmpty() && loaded.mModel ) {
                        if ( !state->mMaterialSphere ) {
                            state->mMaterialSphere = loaded.mModel;
                        }

                        loaded.mModel = state->mMaterialSphere;
                    }

                    const auto found{ state->mEntries.find( path ) };

                    if ( found != state->mEntries.end() && found->second.mGeneration == generation ) {
                        found->second.mRequest = eastl::move( loaded.mRequest );
                        found->second.mReady = eastl::move( loaded.mImage );
                        found->second.mReadyModel = eastl::move( loaded.mModel );
                        found->second.mCachePath = loaded.mCachePath;
                        found->second.mMaterialPath = loaded.mMaterialPath;

                        if ( found->second.mRequest.GetState() != asset::AssetState::eUnloaded ) {
                            found->second.mAwaitingModel = true;
                            ++state->mPendingModels;
                        }
                        else if ( found->second.mReadyModel || !found->second.mMaterialPath.IsEmpty() ) {
                            state->mModelQueue.push_back( path );
                        }
                        else if ( found->second.mReady ) {
                            state->mUploadQueue.push_back( path );
                        }
                    }
                } catch ( const std::exception& error ) {
                    {
                        std::lock_guard lock{ state->mMutex };
                        const auto found{ state->mEntries.find( path ) };

                        if ( found != state->mEntries.end() && found->second.mGeneration == generation ) {
                            found->second.mFailed = true;
                            found->second.mRetryAfter = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
                        }
                    }

                    MKT_CORE_LOGGER_WARN( "Thumbnail {}: {}", path.GetC_Str(), error.what() );
                }

                std::lock_guard lock{ state->mMutex };
                --state->mActiveJobs;
            } );
        } catch ( const std::exception& ) {
            std::lock_guard lock{ state->mMutex };
            --state->mActiveJobs;
            state->mEntries.erase( path );
            throw;
        }
    }

    auto ThumbnailCache::UploadReady() -> void {
        MKT_BEGIN_PROFILER();
        mThumbnailRenderer->Poll();

        if ( mState->mCompressTextures != asset::AssetRegistryService::Get()->IsTextureCompressionEnabled() ) {
            // Old jobs retain their own state and policy; their completions cannot
            // publish thumbnails into the new project's cache.
            auto replacement{ eastl::make_shared<CacheState>() };
            {
                std::lock_guard lock{ mState->mMutex };
                replacement->mDirectory = mState->mDirectory;
            }

            mState = eastl::move( replacement );
        }

        {
            const std::lock_guard lock{ mState->mMutex };
            const auto now{ std::chrono::steady_clock::now() };

            for ( auto it{ mState->mEntries.begin() }; it != mState->mEntries.end(); ) {
                auto& entry{ it->second };
                // The browser touches visible entries each frame. Abandoned
                // requests must not keep importing models after leaving a folder.
                if ( now - entry.mLastRequested > std::chrono::seconds{ 30 } ) {
                    mState->mPendingModels -= entry.mAwaitingModel ? 1 : 0;
                    it = mState->mEntries.erase( it );
                    continue;
                }

                if ( entry.mAwaitingModel ) {
                    const auto status{ entry.mRequest.GetState() };

                    if ( status == asset::AssetState::eReady ) {
                        if ( !entry.mReadyModel ) {
                            entry.mReadyModel = entry.mRequest.GetIfReady();

                            for ( core::usize mesh{}; mesh < entry.mReadyModel->GetMeshNodeCount(); ++mesh ) {
                                for ( const auto& [path, binding]: entry.mReadyModel->GetMeshNode( mesh ).GetProperties().mTextureBindings ) {
                                    if ( !binding.mTexture && !path.IsEmpty() ) {
                                        entry.mTextureRequests.push_back( asset::AssetRegistryService::Get()->Request<asset::Texture>( asset::TextureLoadDescription{ .mPath = path, .mPriority = LoadPriority::eBackground } ) );
                                    }
                                }
                            }
                        }

                        bool pending{};

                        for ( const auto& request: entry.mTextureRequests ) {
                            const auto textureState{ request.GetState() };
                            pending |= textureState == asset::AssetState::eQueued || textureState == asset::AssetState::eLoading;
                        }

                        if ( !pending ) {
                            entry.mRequest = {};
                            entry.mAwaitingModel = false;
                            --mState->mPendingModels;
                            mState->mModelQueue.push_back( it->first );
                        }
                    }
                    else if ( status == asset::AssetState::eFailed || status == asset::AssetState::eUnloaded ) {
                        entry.mRequest = {};
                        entry.mAwaitingModel = false;
                        --mState->mPendingModels;
                        entry.mFailed = true;
                        entry.mRetryAfter = now + std::chrono::seconds{ 5 };
                    }
                }

                ++it;
            }
        }

        if ( !mThumbnailRenderer->IsBusy() ) {
            const auto state{ mState };
            filesystem::Path path{}, cached{}, materialPath{};
            asset::ModelHandle model{};
            core::u64 generation{};
            {
                std::lock_guard lock{ state->mMutex };

                while ( !state->mModelQueue.empty() && !model && materialPath.IsEmpty() ) {
                    path = eastl::move( state->mModelQueue.front() );
                    state->mModelQueue.pop_front();
                    const auto found{ state->mEntries.find( path ) };

                    if ( found != state->mEntries.end() ) {
                        model = eastl::move( found->second.mReadyModel );
                        cached = found->second.mCachePath;
                        materialPath = eastl::move( found->second.mMaterialPath );
                        generation = found->second.mGeneration;
                    }
                }

                if ( model || !materialPath.IsEmpty() ) {
                    ++state->mGpuJobs;
                }
            }

            if ( model || !materialPath.IsEmpty() ) {
                try {
                    const auto complete{ [state, path, cached, generation, compress = state->mCompressTextures]( asset::ImageHandle image ) {
                        // Readback owns its pixel copy. Disk encoding can run
                        // on a worker after the GPU targets are recycled.
                        const auto publish{ [state, path, generation]( asset::ImageHandle ready ) {
                            std::lock_guard lock{ state->mMutex };
                            --state->mGpuJobs;
                            const auto found{ state->mEntries.find( path ) };

                            if ( found == state->mEntries.end() || found->second.mGeneration != generation ) {
                                return;
                            }

                            found->second.mTextureRequests.clear();

                            if ( ready ) {
                                found->second.mReady = ready;
                                state->mUploadQueue.push_back( path );
                            }
                            else {
                                found->second.mFailed = true;
                                found->second.mRetryAfter = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
                            }
                        } };

                        if ( !image ) {
                            publish( {} );
                            return;
                        }

                        try {
                            threading::TaskService::Get()->ExecuteWorkerTask( [image, cached, publish, compress]() mutable {
                                try {
                                    image = SavePreview( image, cached, compress );
                                } catch ( const std::exception& error ) {
                                    MKT_CORE_LOGGER_WARN( "Preview cache: {}", error.what() );
                                }

                                publish( image );
                            } );
                        } catch ( const std::exception& ) {
                            publish( image );
                        }
                    } };
                    material::MaterialHandle asset{};

                    if ( !materialPath.IsEmpty() ) {
                        // Texture creation stays on the device/context thread.
                        asset = material::LoadMaterial( filesystem::FileHandle::New( materialPath ) );
                    }

                    // Use a fixed HDR panorama so browser thumbnails have stable
                    // lighting regardless of the open scene. Load it once on the
                    // import worker; image/file thumbnails never need it.
                    ThumbnailPreviewSettings settings{};
                    {
                        const std::lock_guard lock{ state->mMutex };
                        settings.mEnvironment = state->mEnvironment;
                    }

                    settings.mBackgroundBlur = 3.0f;
                    settings.mGround = false;
                    // Let material spheres fill most of the tile; models use the bounds fit below.
                    settings.mDistance = 3.4f;
                    settings.mFitMaterialSphere = static_cast<bool>( asset );

                    if ( !asset ) {
                        // Show more of the facade and less roof than the old
                        // elevated orbit, while retaining a little side depth.
                        settings.mYaw = 20.0f;
                        settings.mPitch = 8.0f;
                        settings.mFrameModelBounds = true;
                    }

                    const bool started{ mThumbnailRenderer->BeginModelPreview( model, settings, asset, complete ) };

                    if ( !started ) {
                        MKT_THROW_RUNTIME_ERROR( "GPU preview renderer is busy." );
                    }
                } catch ( const std::exception& error ) {
                    std::lock_guard lock{ state->mMutex };
                    --state->mGpuJobs;
                    const auto found{ state->mEntries.find( path ) };

                    if ( found != state->mEntries.end() && found->second.mGeneration == generation ) {
                        found->second.mFailed = true;
                        found->second.mRetryAfter = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
                    }

                    MKT_CORE_LOGGER_WARN( "GPU thumbnail {}: {}", path.GetC_Str(), error.what() );
                }
            }
        }

        // Limit uploads per frame so entering an image-heavy folder stays responsive.
        for ( core::u32 uploaded{}; uploaded < 2; ++uploaded ) {
            filesystem::Path path;
            asset::ImageHandle ready{};
            core::u64 generation{};
            {
                std::lock_guard lock{ mState->mMutex };

                if ( !mState->mUploadQueue.empty() ) {
                    path = eastl::move( mState->mUploadQueue.front() );
                    mState->mUploadQueue.pop_front();
                    const auto found{ mState->mEntries.find( path ) };

                    if ( found != mState->mEntries.end() ) {
                        generation = found->second.mGeneration;
                        ready = eastl::move( found->second.mReady );
                    }
                }
            }

            if ( ready.IsEmpty() ) {
                break;
            }

            try {
                auto texture{ mThumbnailRenderer->UploadPreview( eastl::move( ready ) ) };

                if ( texture.IsEmpty() ) {
                    MKT_THROW_RUNTIME_ERROR( "Thumbnail texture upload failed." );
                }

                std::lock_guard lock{ mState->mMutex };
                const auto found{ mState->mEntries.find( path ) };

                if ( found != mState->mEntries.end() && found->second.mGeneration == generation ) {
                    found->second.mThumbnail.mThumbnail = eastl::move( texture );
                }
            } catch ( const std::exception& error ) {
                {
                    std::lock_guard lock{ mState->mMutex };
                    const auto found{ mState->mEntries.find( path ) };

                    if ( found != mState->mEntries.end() && found->second.mGeneration == generation ) {
                        found->second.mFailed = true;
                        found->second.mRetryAfter = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
                    }
                }

                MKT_CORE_LOGGER_WARN( "Thumbnail upload {}: {}", path.GetC_Str(), error.what() );
            }
        }
    }


}// namespace mikoto::editor
