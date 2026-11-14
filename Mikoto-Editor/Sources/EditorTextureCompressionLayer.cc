// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License").

#include <mutex>
#include <chrono>
#include <thread>
#include <cstring>
#include <filesystem>

#include <imgui.h>

#include <EASTL/sort.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <implot.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>

#include <Platform/CoreEvents.hh>

#include <Assets/Asset.hh>
#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Layers/EditorTextureCompressionLayer.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace asset;
    using namespace filesystem;


    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;
    static constexpr core::f64 kMiB{ 1024.0 * 1024.0 };

    struct BenchmarkFile {
        filesystem::Path mPath{};
        eastl::string mLabel{};
        bool mSelected{ true };
    };

    struct BenchmarkResult {
        eastl::string mName{};
        eastl::string mFormat{};
        eastl::string mError{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::usize mSourceBytes{};
        core::usize mDecodedBytes{};
        core::usize mEncodedBytes{};
        core::usize mCacheBytes{};
        core::i32 mSamples{};
        core::f64 mLoadMs{};
        core::f64 mEncodeMs{};
        core::f64 mWriteMs{};
        core::f64 mReadMs{};
    };

    template<typename Function>
    MKT_NODISCARD static auto Measure( Function&& operation ) -> core::f64 {
        const auto begin{ Clock::now() };
        operation();
        return std::chrono::duration<core::f64, std::milli>{ Clock::now() - begin }.count();
    }

    struct BenchmarkLabels {
        eastl::string mFloatRetained{};
        eastl::string mByteRetained{};
        eastl::string mUnsupported{};
        eastl::string mAlreadyEncoded{};

        eastl::string mCancelled{};
        eastl::string mFinished{};
    };

    MKT_NODISCARD static auto FormatName( asset::ImageFormat format, const BenchmarkLabels& labels ) -> cstr {
        switch ( format ) {
            case asset::ImageFormat::eCompressed_BC6:
                return "BC6H UF16";
            case asset::ImageFormat::eCompressed_BC7:
                return "BC7";
            case asset::ImageFormat::eRGBA32_FLOAT:
                return labels.mFloatRetained.c_str();
            case asset::ImageFormat::eRGBA8_UINT:
                return labels.mByteRetained.c_str();
            default:
                return labels.mUnsupported.c_str();
        }
    }

    // Timings exclude verification and UI work. Each iteration opens a fresh
    // File, bypassing the asset registry and the normal derived texture cache.
    MKT_NODISCARD static auto BenchmarkImage( const BenchmarkFile& file, const filesystem::Path& cached, core::i32 iterations, std::stop_token stop, const BenchmarkLabels& labels ) -> BenchmarkResult {
        BenchmarkResult result{};
        result.mName = file.mLabel;

        try {
            result.mSourceBytes = fs::file_size( file.mPath.GetPathTyped<fs::path>() );

            for ( core::i32 iteration{}; iteration < iterations && !stop.stop_requested(); ++iteration ) {
                asset::ImageHandle original{}, encoded{}, reloaded{};
                const core::f64 loadMs{ Measure( [&] {
                    original = asset::ProcessImage2D( filesystem::FileHandle::New( file.mPath ) );
                } ) };

                if ( stop.stop_requested() ) {
                    break;
                }

                const core::f64 encodeMs{ Measure( [&] {
                    encoded = asset::CompressImage( original );
                } ) };

                if ( stop.stop_requested() ) {
                    break;
                }

                const core::f64 writeMs{ Measure( [&] {
                    asset::WriteImageCompressed( cached, encoded );
                } ) };

                if ( stop.stop_requested() ) {
                    break;
                }

                const core::f64 readMs{ Measure( [&] {
                    reloaded = asset::ProcessImageCompressed( filesystem::FileHandle::New( cached ) );
                } ) };

                if ( reloaded->mFormat != encoded->mFormat || reloaded->mWidth != encoded->mWidth ||
                     reloaded->mHeight != encoded->mHeight || reloaded->mBufferSpan->GetSize() != encoded->mBufferSpan->GetSize() ||
                     std::memcmp( reloaded->mBufferSpan->GetData(), encoded->mBufferSpan->GetData(), encoded->mBufferSpan->GetSize() ) != 0 ) {
                    MKT_THROW_RUNTIME_ERROR( "KTX2 round-trip verification failed." );
                }

                result.mWidth = original->mWidth;
                result.mHeight = original->mHeight;
                result.mFormat = FormatName( encoded->mFormat, labels );

                if ( original->mFormat == asset::ImageFormat::eCompressed_BC6 || original->mFormat == asset::ImageFormat::eCompressed_BC7 ) {
                    result.mFormat += labels.mAlreadyEncoded.c_str();
                }

                result.mDecodedBytes = original->mBufferSpan->GetSize();
                result.mEncodedBytes = encoded->mBufferSpan->GetSize();
                result.mCacheBytes = fs::file_size( cached.GetPathTyped<fs::path>() );
                result.mLoadMs += loadMs;
                result.mEncodeMs += encodeMs;
                result.mWriteMs += writeMs;
                result.mReadMs += readMs;
                ++result.mSamples;
            }
        } catch ( const std::exception& error ) {
            result.mError = error.what();
        }

        if ( result.mSamples ) {
            result.mLoadMs /= result.mSamples;
            result.mEncodeMs /= result.mSamples;
            result.mWriteMs /= result.mSamples;
            result.mReadMs /= result.mSamples;
        }

        return result;
    }

    MKT_NODISCARD static auto CsvField( eastl::string_view value ) -> eastl::string {
        eastl::string escaped{ "\"" };

        for ( char character: value ) {
            if ( character == '"' ) {
                escaped += '"';
            }

            escaped += character;
        }

        escaped += '"';
        return escaped;
    }

    static auto WriteReport( const filesystem::Path& path, const eastl::vector<BenchmarkResult>& results ) -> void {
        eastl::string csv{ "file,format,width,height,samples,source_bytes,input_payload_bytes,output_payload_bytes,ktx2_bytes,load_mean_ms,encode_mean_ms,write_mean_ms,reload_mean_ms,error\n" };

        for ( const auto& row: results ) {
            csv += string::Format( "{},{},{},{},{},{},{},{},{},{:.6f},{:.6f},{:.6f},{:.6f},{}\n",
                CsvField( row.mName ), CsvField( row.mFormat ), row.mWidth, row.mHeight, row.mSamples,
                row.mSourceBytes, row.mDecodedBytes, row.mEncodedBytes, row.mCacheBytes,
                row.mLoadMs, row.mEncodeMs, row.mWriteMs, row.mReadMs, CsvField( row.mError ) );
        }

        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open texture compression report for writing." );
        }

        file->Write( csv );
    }


    struct EditorTextureCompressionLayer::State {
        eastl::vector<BenchmarkFile> mFiles{};
        std::mutex mMutex{};
        eastl::vector<BenchmarkResult> mResults{};
        eastl::string mStatus{};
        eastl::string mError{};
        filesystem::Path mReport{};
        core::usize mTotal{};
        bool mRunning{};
        // Declared last so destruction joins before any worker-owned data goes away.
        std::jthread mWorker{};
    };

    EditorTextureCompressionLayer::EditorTextureCompressionLayer()
        : mName{ "EditorTextureCompressionLayer" }, mState{ eastl::make_unique<State>() } {
    }

    EditorTextureCompressionLayer::~EditorTextureCompressionLayer() = default;

    auto EditorTextureCompressionLayer::OnCreate() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing EditorTextureCompressionLayer (F9)..." );
        ScanResources();
    }

    auto EditorTextureCompressionLayer::OnDestroy() -> void {
        if ( mState->mWorker.joinable() ) {
            mState->mWorker.request_stop();
            mState->mWorker.join();
        }

        MKT_CORE_LOGGER_INFO( "Shutting down EditorTextureCompressionLayer..." );
    }

    auto EditorTextureCompressionLayer::ScanResources() -> void {
        mState->mFiles.clear();
        mState->mError.clear();

        try {
            fs::path root{ "Resources" };

            if ( !fs::is_directory( root ) ) {
                root = "Mikoto/Resources/Resources";
            }

            for ( core::cstr folder: { "Textures", "Cubemaps" } ) {
                const auto directory{ root / folder };

                if ( !fs::is_directory( directory ) ) {
                    continue;
                }

                for ( const auto& entry: fs::recursive_directory_iterator{ directory } ) {
                    const filesystem::Path path{ entry.path() };

                    if ( entry.is_regular_file() && asset::IsFileImage( path ) ) {
                        mState->mFiles.push_back( { path.ToAbsolute(), entry.path().lexically_relative( root ).generic_string().c_str() } );
                    }
                }
            }

            eastl::sort( mState->mFiles.begin(), mState->mFiles.end(), []( const auto& left, const auto& right ) {
                return left.mLabel < right.mLabel;
            } );

            if ( mState->mFiles.empty() ) {
                mState->mError = MKT_LOC( "editor_texture_compression_layer_no_supported_images_found_in_resources_textures_or_resources_cubemaps" ).c_str();
            }
        } catch ( const std::exception& error ) {
            mState->mError = error.what();
        }
    }

    auto EditorTextureCompressionLayer::StartBenchmark() -> void {
        {
            std::lock_guard lock{ mState->mMutex };

            if ( mState->mRunning ) {
                return;
            }
        }

        if ( mState->mWorker.joinable() ) {
            mState->mWorker.join();
        }

        eastl::vector<BenchmarkFile> files{};

        for ( const auto& file: mState->mFiles ) {
            if ( file.mSelected ) {
                files.push_back( file );
            }
        }

        if ( files.empty() ) {
            return;
        }

        const auto run{ std::chrono::system_clock::now().time_since_epoch().count() };
        const filesystem::Path directory{ fs::absolute( fs::path{ "Assets/.cache/Benchmarks/TextureCompression" } / string::Format( "{}", run ).c_str() ) };
        {
            std::lock_guard lock{ mState->mMutex };
            mState->mResults.clear();
            mState->mError.clear();
            mState->mReport = {};
            mState->mTotal = files.size();
            mState->mStatus = MKT_LOC( "editor_texture_compression_layer_starting" );
            mState->mRunning = true;
        }

        // Copy UI strings before dispatch: the worker must not read the language
        // selection while the editor thread can change it.
        const BenchmarkLabels labels{
            MKT_LOC( "editor_texture_compression_layer_rgba32f_retained" ),
            MKT_LOC( "editor_texture_compression_layer_rgba8_retained" ),
            MKT_LOC( "editor_texture_compression_layer_unsupported" ),
            MKT_LOC( "editor_texture_compression_layer_already_encoded" ),
            MKT_LOC( "editor_texture_compression_layer_cancelled_completed_samples_saved" ),
            MKT_LOC( "editor_texture_compression_layer_finished_csv_saved" )
        };

        try {
            mState->mWorker = std::jthread{ [state = mState.get(), files = eastl::move( files ), directory, iterations = mIterations, labels]( std::stop_token stop ) {
                try {
                    fs::create_directories( directory.GetPathTyped<fs::path>() );

                    for ( core::usize index{}; index < files.size() && !stop.stop_requested(); ++index ) {
                        {
                            std::lock_guard lock{ state->mMutex };
                            state->mStatus = files[index].mLabel;
                        }

                        const filesystem::Path cached{ directory.GetPathTyped<fs::path>() / string::Format( "{}.ktx2", index ).c_str() };
                        auto result{ BenchmarkImage( files[index], cached, iterations, stop, labels ) };

                        if ( result.mSamples || !result.mError.empty() ) {
                            std::lock_guard lock{ state->mMutex };
                            state->mResults.push_back( eastl::move( result ) );
                        }
                    }

                    const filesystem::Path report{ directory.GetPathTyped<fs::path>() / "results.csv" };

                    // Only this worker mutates results. Readers take snapshots under the mutex.
                    WriteReport( report, state->mResults );

                    std::lock_guard lock{ state->mMutex };
                    state->mReport = report;
                    state->mStatus = stop.stop_requested() ? labels.mCancelled : labels.mFinished;
                } catch ( const std::exception& error ) {
                    std::lock_guard lock{ state->mMutex };
                    state->mError = error.what();
                }

                std::lock_guard lock{ state->mMutex };
                state->mRunning = false;
            } };
        } catch ( const std::exception& error ) {
            std::lock_guard lock{ mState->mMutex };
            mState->mError = error.what();
            mState->mRunning = false;
        }
    }

    auto EditorTextureCompressionLayer::OnRender( float ) -> void {
    }

    auto EditorTextureCompressionLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorTextureCompressionLayer::OnEvent( platform::Event& event ) -> void {
        if ( event.IsType( platform::EventType::KEY_PRESSED_EVENT ) ) {
            const auto& key{ static_cast<const platform::KeyPressedEvent&>( event ) };

            if ( key.GetKeyCode() == platform::KeyCode::Key_F9 && !key.IsRepeated() ) {
                mVisible = !mVisible;
                event.SetHandled( true );
            }
        }
    }

    auto EditorTextureCompressionLayer::OnUpdate( float ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mVisible ) {
            DisplayWindow();
        }
    }

    auto EditorTextureCompressionLayer::DisplayWindow() -> void {
        ImGui::SetNextWindowSize( { 1120, 760 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_texture_compression_benchmark_editortexturecompressionlayer", "Texture Compression Benchmark##EditorTextureCompressionLayer" ).c_str(), &mVisible ) ) {
            ImGui::End();
            return;
        }

        eastl::vector<BenchmarkResult> results{};
        eastl::string status{}, error{};
        filesystem::Path report{};
        bool running{};
        core::usize total{};
        {
            std::lock_guard lock{ mState->mMutex };
            results = mState->mResults;
            status = mState->mStatus;
            error = mState->mError;
            report = mState->mReport;
            running = mState->mRunning;
            total = mState->mTotal;
        }

        ImGui::TextUnformatted( MKT_LOC( "editor_texture_compression_layer_bc7_bc6h_cpu_import_benchmark_f9_toggles_this_window" ).c_str() );
        ImGui::Text( MKT_LOC( "editor_texture_compression_layer_compression_workers_assets_texture_compression_workers_restart_to_change" ).c_str(), asset::GetTextureCompressionWorkerCount() );
        ImGui::TextWrapped( MKT_LOC( "editor_texture_compression_layer_uses_the_production_encoder_regardless_of_compresstextures_full_resolution_mip" ).c_str() );
#if !defined( NDEBUG )
        ImGui::TextColored( { 1.0f, 0.75f, 0.25f, 1.0f }, MKT_LOC( "editor_texture_compression_layer_debug_build_use_release_for_representative_compression_timings" ).c_str() );
#endif
        ImGui::TextWrapped( MKT_LOC( "editor_texture_compression_layer_times_are_sample_means_without_a_discarded_warm_up_the" ).c_str() );

        bool start{};
        ImGui::BeginDisabled( running );
        ImGui::SetNextItemWidth( 140 );
        ImGui::SliderInt( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_samples_per_file", "Samples per file" ).c_str(), &mIterations, 1, 10 );
        start = ImGui::Button( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_run_selected", "Run selected" ).c_str() );

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_select_all", "Select all" ).c_str() ) ) {
            for ( auto& file: mState->mFiles ) {
                file.mSelected = true;
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_select_none", "Select none" ).c_str() ) ) {
            for ( auto& file: mState->mFiles ) {
                file.mSelected = false;
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_rescan", "Rescan" ).c_str() ) ) {
            ScanResources();
        }

        if ( ImGui::BeginChild( "Inputs", { 0, 145 }, ImGuiChildFlags_Borders ) ) {
            for ( auto& file: mState->mFiles ) {
                imgui::CheckBox( file.mLabel.c_str(), file.mSelected );
            }
        }

        ImGui::EndChild();
        ImGui::EndDisabled();

        if ( start ) {
            StartBenchmark();
            std::lock_guard lock{ mState->mMutex };
            running = mState->mRunning;
        }

        if ( running && ImGui::Button( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_cancel_after_current_stage", "Cancel after current stage" ).c_str() ) ) {
            mState->mWorker.request_stop();
        }

        ImGui::TextWrapped( "%s", status.c_str() );

        if ( total ) {
            ImGui::ProgressBar( core::f32( results.size() ) / core::f32( total ), { -1, 0 } );
        }

        if ( !error.empty() ) {
            ImGui::TextWrapped( "%s", error.c_str() );
        }

        if ( !report.IsEmpty() ) {
            ImGui::TextWrapped( MKT_LOC( "editor_texture_compression_layer_report" ).c_str(), report.GetC_Str() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "editor_texture_compression_layer_copy_report_path", "Copy report path" ).c_str() ) ) {
                ImGui::SetClipboardText( report.GetC_Str() );
            }
        }

        if ( ImGui::BeginTable( "Results", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, { 0, 220 } ) ) {
            const core::cstr columnLabels[]{
                MKT_LOC( "editor_texture_compression_layer_file" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_format" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_dimensions" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_input_mib" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_output_mib" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_ratio" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_load_ms" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_encode_ms" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_write_ms" ).c_str(),
                MKT_LOC( "editor_texture_compression_layer_reload_ms" ).c_str()
            };

            for ( core::cstr label: columnLabels ) {
                ImGui::TableSetupColumn( label, ImGuiTableColumnFlags_WidthFixed, 100 );
            }

            ImGui::TableSetupScrollFreeze( 1, 1 );
            ImGui::TableHeadersRow();

            for ( const auto& row: results ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted( row.mName.c_str() );
                imgui::widget::ItemTooltip( MKT_LOC( "editor_texture_compression_layer_samples_source_file_mib_ktx2_file_mib" ).c_str(), row.mName.c_str(), row.mSamples, row.mSourceBytes / kMiB, row.mCacheBytes / kMiB, row.mError.c_str() );
                ImGui::TableNextColumn();
                ImGui::TextUnformatted( row.mError.empty() ? row.mFormat.c_str() : MKT_LOC( "editor_texture_compression_layer_error_hover_file" ).c_str() );
                ImGui::TableNextColumn();
                ImGui::Text( "%u x %u", row.mWidth, row.mHeight );
                ImGui::TableNextColumn();
                ImGui::Text( "%.3f", row.mDecodedBytes / kMiB );
                ImGui::TableNextColumn();
                ImGui::Text( "%.3f", row.mEncodedBytes / kMiB );
                ImGui::TableNextColumn();
                ImGui::Text( "%.2fx", row.mEncodedBytes ? core::f64( row.mDecodedBytes ) / row.mEncodedBytes : 0.0 );
                ImGui::TableNextColumn();
                ImGui::Text( "%.2f", row.mLoadMs );
                ImGui::TableNextColumn();
                ImGui::Text( "%.2f", row.mEncodeMs );
                ImGui::TableNextColumn();
                ImGui::Text( "%.2f", row.mWriteMs );
                ImGui::TableNextColumn();
                ImGui::Text( "%.2f", row.mReadMs );
            }

            ImGui::EndTable();
        }

        ImGui::TextDisabled( MKT_LOC( "editor_texture_compression_layer_payload_sizes_exclude_gpu_allocation_padding_mipmaps_rgba32f_retained_hdr" ).c_str() );

        if ( !results.empty() && ImPlot::BeginPlot( MKT_LOC( "editor_texture_compression_layer_encoding_time" ).c_str(), { -1, 190 } ) ) {
            eastl::vector<double> times{};

            for ( const auto& row: results ) {
                times.push_back( row.mSamples && row.mError.empty() ? row.mEncodeMs : 0.0 );
            }

            ImPlot::SetupAxes( MKT_LOC( "editor_texture_compression_layer_result_row_zero_based" ).c_str(), MKT_LOC( "editor_texture_compression_layer_mean_encode_time_ms" ).c_str() );
            ImPlot::PlotBars( MKT_LOC( "editor_texture_compression_layer_encoding" ).c_str(), times.data(), static_cast<int>( times.size() ) );
            ImPlot::EndPlot();
        }

        ImGui::End();
    }
}
