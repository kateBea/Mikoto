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

#include <regex>
#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <fstream>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>

#include <cpptrace/cpptrace.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

namespace fs = std::filesystem;

namespace mikoto::logging {

    using namespace mikoto::core;

    // Global logger instance
    DefaultLogger sLogger{};

    MKT_NODISCARD static auto GetNextLogFilePath( const fs::path& directory ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        constexpr std::string_view kBase{ "mikoto" };
        constexpr std::string_view kExtension{ ".log" };

        const std::string today{ fmt::format( "{:%Y%m%d}", std::chrono::system_clock::now() ) };

        // Regex pattern to match log files like mikoto-20251014-1.log,
        // mikoto, then date, then log count index
        i32 maxIndex{};
        const std::regex pattern{ R"(mikoto-(\d{8})-(\d+)\.log)" };

        // Find the last file index
        for ( const auto& entry: std::views::all( fs::directory_iterator( directory ) ) |
                                     std::views::filter( []( auto& e ) {
            return e.is_regular_file();
        } ) ) {
            if ( entry.path().extension() != kExtension ) {
                continue;
            }

            std::smatch match{};
            const std::string filename{ entry.path().filename().string() };

            if ( std::regex_match( filename, match, pattern ) && match[1].str() == today ) {
                maxIndex = eastl::max( maxIndex, std::stoi( match[2].str() ) );
            }
        }

        return string::ConvertToEastl( ( directory / fmt::format( "{}-{}-{}{}", kBase, today, maxIndex + 1, kExtension ) ).string() );
    }

    MKT_NODISCARD static auto ToSpdlogLevel( Severity severity ) -> spdlog::level::level_enum {
        switch ( severity ) {
            case Severity::eTrace:
                return spdlog::level::trace;
            case Severity::eDebug:
                return spdlog::level::debug;
            case Severity::eInfo:
                return spdlog::level::info;
            case Severity::eWarning:
                return spdlog::level::warn;
            case Severity::eError:
                return spdlog::level::err;
            case Severity::eCritical:
                return spdlog::level::critical;
        }

        MKT_THROW_INVALID_ARGUMENT( "Invalid logging severity." );
    }

    DefaultLogger::DefaultLogger() {
        MKT_BEGIN_PROFILER();
        Initialize();
    }

    auto DefaultLogger::GetPtr() -> DefaultLogger* {
        return checked_cast<DefaultLogger*>( ILogger::GetPtr() );
    }

    auto DefaultLogger::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        try {
            mStdOut = spdlog::stdout_color_mt( "MIKOTO_STDOUT_LOGGER" );
            mStdErr = spdlog::stderr_color_mt( "MIKOTO_STDERR_LOGGER" );

            // Install the observer sink before workers start logging. Sink arrays stay immutable afterwards.
            auto consoleSink{ std::make_shared<spdlog::sinks::callback_sink_mt>( [this]( const spdlog::details::log_msg& message ) {
                std::lock_guard lock{ mConsoleObserverMutex };

                if ( !mConsoleObserver ) {
                    return;
                }

                Severity severity{ Severity::eDebug };

                switch ( message.level ) {
                    case spdlog::level::trace:
                        severity = Severity::eTrace;
                        break;
                    case spdlog::level::debug:
                        severity = Severity::eDebug;
                        break;
                    case spdlog::level::info:
                        severity = Severity::eInfo;
                        break;
                    case spdlog::level::warn:
                        severity = Severity::eWarning;
                        break;
                    case spdlog::level::err:
                        severity = Severity::eError;
                        break;
                    case spdlog::level::critical:
                        severity = Severity::eCritical;
                        break;
                    default:
                        return;
                }

                mConsoleObserver( severity, eastl::string_view{ message.payload.data(), message.payload.size() } );
            } ) };

            mStdOut->sinks().push_back( consoleSink );
            mStdErr->sinks().push_back( consoleSink );

            // Keep the existing per-run log file available to explicit file logging calls.
            constexpr eastl::string_view logsPath{ "Logs" };
            ( void )filesystem::CreateIfNotExistsDirectory( logsPath );

            if ( mFileLogName.empty() ) {
                mFileLogName = GetNextLogFilePath( logsPath.data() );
            }

            mFile = spdlog::basic_logger_mt( "MIKOTO_FILE_LOGGER", mFileLogName.c_str() );

            mStdOut->set_pattern( mPattern.c_str() );
            mStdErr->set_pattern( mPattern.c_str() );
            mFile->set_pattern( mPattern.c_str() );

            SetLevel( mLoggingSeverity );
            FlushOn( mFlushSeverity );
            mIsInitialized = true;
        } catch ( const spdlog::spdlog_ex& exception ) {
            Shutdown();
            MKT_THROW_RUNTIME_ERROR( string::Format( "Failed to initialize logger: {}", exception.what() ) );
        } catch ( ... ) {
            Shutdown();
            throw;
        }
    }

    auto DefaultLogger::GetStdOutLog() const -> std::shared_ptr<spdlog::logger> {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mStateMutex };

        MKT_ASSERT( mStdOut, "STD Out Logger is NULL" );

        return mStdOut;
    }

    auto DefaultLogger::GetStdErrLog() const -> std::shared_ptr<spdlog::logger> {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mStateMutex };

        MKT_ASSERT( mStdErr, "STD Err Logger is NULL" );

        return mStdErr;
    }

    auto DefaultLogger::SetConsoleObserver( eastl::function<void( Severity, eastl::string_view )> observer ) -> void {
        std::lock_guard lock{ mConsoleObserverMutex };
        mConsoleObserver = eastl::move( observer );
    }

    auto DefaultLogger::Log( Severity severity, eastl::string_view message ) -> void {
        MKT_BEGIN_PROFILER();

        // Get our severity type
        const auto level{ ToSpdlogLevel( severity ) };

        // Get the proper type of logger, file or console
        std::shared_ptr<spdlog::logger> logger{};
        {
            std::lock_guard lock{ mStateMutex };

            if ( !mIsInitialized ) {
                return;
            }

            logger = mOutput == Output::eFile ? mFile : ( severity >= Severity::eError ? mStdErr : mStdOut );
        }

        // Keep the selected logger alive through output changes. Do not hold the state lock during observer callbacks.
        // Pass a sized payload so substrings, embedded nulls and braces remain literal text.
        logger->log( level, spdlog::string_view_t{ message.empty() ? "" : message.data(), message.size() } );
    }

    auto DefaultLogger::Log( Severity severity, const core::Exception& exception ) -> void {
        const auto& reason{ exception.GetReason() };
        Log( severity, eastl::string_view{ reason.data(), reason.size() } );
    }

    auto DefaultLogger::LogTrace( eastl::string_view message ) -> void {
        Log( Severity::eTrace, message );
    }

    auto DefaultLogger::LogDebug( eastl::string_view message ) -> void {
        Log( Severity::eDebug, message );
    }

    auto DefaultLogger::LogError( eastl::string_view message ) -> void {
        Log( Severity::eError, message );
    }

    auto DefaultLogger::LogWarning( eastl::string_view message ) -> void {
        Log( Severity::eWarning, message );
    }

    auto DefaultLogger::LogCritical( eastl::string_view message ) -> void {
        Log( Severity::eCritical, message );
    }

    auto DefaultLogger::LogInformation( eastl::string_view message ) -> void {
        Log( Severity::eInfo, message );
    }

    auto DefaultLogger::LogTrace( const core::Exception& exception ) -> void {
        Log( Severity::eTrace, exception );
    }

    auto DefaultLogger::LogDebug( const core::Exception& exception ) -> void {
        Log( Severity::eDebug, exception );
    }

    auto DefaultLogger::LogError( const core::Exception& exception ) -> void {
        Log( Severity::eError, exception );
    }

    auto DefaultLogger::LogWarning( const core::Exception& exception ) -> void {
        Log( Severity::eWarning, exception );
    }

    auto DefaultLogger::LogCritical( const core::Exception& exception ) -> void {
        Log( Severity::eCritical, exception );
    }

    auto DefaultLogger::LogInformation( const core::Exception& exception ) -> void {
        Log( Severity::eInfo, exception );
    }

    auto DefaultLogger::SetLevel( Severity severity ) -> void {
        MKT_BEGIN_PROFILER();
        const auto level{ ToSpdlogLevel( severity ) };
        std::lock_guard lock{ mStateMutex };

        for ( const auto& logger: { mStdOut, mStdErr, mFile } ) {
            if ( logger ) {
                logger->set_level( level );
            }
        }

        mLoggingSeverity = severity;
    }

    auto DefaultLogger::SetPattern( eastl::string_view pattern ) -> void {
        MKT_BEGIN_PROFILER();
        const std::string ownedPattern{ pattern.empty() ? "" : pattern.data(), pattern.size() };
        eastl::array<std::shared_ptr<spdlog::logger>, 3> loggers{};
        {
            std::lock_guard lock{ mStateMutex };
            mPattern.assign( ownedPattern.data(), ownedPattern.size() );
            loggers = { mStdOut, mStdErr, mFile };
        }

        for ( const auto& logger: loggers ) {
            if ( logger ) {
                logger->set_pattern( ownedPattern );
            }
        }
    }

    auto DefaultLogger::SetLogOutput( Output output, eastl::string_view path ) -> void {
        MKT_BEGIN_PROFILER();

        if ( output != Output::eConsole && output != Output::eFile ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid logging output." );
        }

        // If it is a file it needs to be a null terminated string, string_view is not guaranteed to be null terminated
        if ( output == Output::eFile && ( path.empty() || path.find( '\0' ) != eastl::string_view::npos ) ) {
            MKT_THROW_INVALID_ARGUMENT( "File logging requires a non-empty path without null characters." );
        }

        std::lock_guard lock{ mStateMutex };

        if ( !mIsInitialized ) {
            MKT_THROW_LOGIC_ERROR( "Cannot change output on a stopped logger." );
        }

        if ( output == Output::eFile && path != eastl::string_view{ mFileLogName } ) {
            try {
                // Prepare a replacement without mutating sinks that another thread may still be using.
                const std::string filePath{ path.data(), path.size() };
                auto sink{ std::make_shared<spdlog::sinks::basic_file_sink_mt>( filePath, false ) };
                auto replacement{ std::make_shared<spdlog::logger>( "MIKOTO_FILE_LOGGER", sink ) };

                replacement->set_level( ToSpdlogLevel( mLoggingSeverity ) );
                replacement->flush_on( ToSpdlogLevel( mFlushSeverity ) );
                replacement->set_pattern( mPattern.c_str() );

                mFile->flush();

                spdlog::drop( mFile->name() );
                spdlog::register_logger( replacement );

                mFile = eastl::move( replacement );
                mFileLogName.assign( path.data(), path.size() );

            } catch ( const spdlog::spdlog_ex& exception ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to set log output: {}", exception.what() ) );
            }
        }

        mOutput = output;
    }

    auto DefaultLogger::Flush() -> void {
        MKT_BEGIN_PROFILER();
        eastl::array<std::shared_ptr<spdlog::logger>, 3> loggers{};
        {
            std::lock_guard lock{ mStateMutex };
            loggers = { mStdOut, mStdErr, mFile };
        }

        for ( const auto& logger: loggers ) {
            if ( logger ) {
                logger->flush();
            }
        }
    }

    auto DefaultLogger::FlushOn( Severity severity ) -> void {
        MKT_BEGIN_PROFILER();

        const auto level{ ToSpdlogLevel( severity ) };
        std::lock_guard lock{ mStateMutex };

        for ( const auto& logger: { mStdOut, mStdErr, mFile } ) {
            if ( logger ) {
                logger->flush_on( level );
            }
        }

        mFlushSeverity = severity;
    }

    auto DefaultLogger::GetLevel() const -> Severity {
        std::lock_guard lock{ mStateMutex };
        return mLoggingSeverity;
    }

    auto DefaultLogger::GetFlushLevel() const -> Severity {
        std::lock_guard lock{ mStateMutex };
        return mFlushSeverity;
    }

    auto DefaultLogger::GetName() const -> eastl::string_view {
        std::lock_guard lock{ mStateMutex };
        // The names are fixed; return stable storage even if the output changes later.
        return mOutput == Output::eFile ? "MIKOTO_FILE_LOGGER" : "MIKOTO_STDOUT_LOGGER";
    }

    auto DefaultLogger::ShouldLog( Severity severity ) const -> bool {
        const auto level{ ToSpdlogLevel( severity ) };
        std::lock_guard lock{ mStateMutex };
        const auto& logger{ mOutput == Output::eFile ? mFile : ( severity >= Severity::eError ? mStdErr : mStdOut ) };
        return mIsInitialized && logger && logger->should_log( level );
    }

    auto DefaultLogger::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        // Detach editor clients before flushing and dropping only this service's registered loggers.
        SetConsoleObserver( {} );
        std::lock_guard lock{ mStateMutex };
        mIsInitialized = false;

        for ( auto* logger: { &mStdOut, &mStdErr, &mFile } ) {
            if ( *logger ) {
                ( *logger )->flush();
                spdlog::drop( ( *logger )->name() );
                logger->reset();
            }
        }
    }

    DefaultLogger::~DefaultLogger() {
        Shutdown();
    }

    auto DefaultLogger::GetStdFileLog() const -> std::shared_ptr<spdlog::logger> {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mStateMutex };
        MKT_ASSERT( mFile, "File Logger is NULL" );
        return mFile;
    }
}// namespace mikoto::logging

namespace mikoto::logging::stacktrace {

    auto ToString() -> eastl::string {
        MKT_BEGIN_PROFILER();

        return cpptrace::generate_trace().to_string().c_str();
    }

}// mikoto
