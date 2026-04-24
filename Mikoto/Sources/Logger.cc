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

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Core/Exception.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

namespace fs = std::filesystem;

namespace mikoto::core {

    // Global logger instance
    Logger sLogger{};

    MKT_NODISCARD
    static auto NextLogFilePath( const fs::path& directory ) -> eastl::string {
        constexpr std::string_view BASE{ "mikoto" };
        constexpr std::string_view EXT{ ".log" };

        const std::string today{ fmt::format( "{:%Y%m%d}", std::chrono::system_clock::now() ) };

        // Regex pattern to match log files like mikoto-20251014-1.log,
        // mikoto, then date, then log count index
        i32 maxIndex{};
        const std::regex pattern{ R"(mikoto-(\d{8})-(\d+)\.log)" };

        // Find the last file index
        for ( const auto& entry : std::views::all( fs::directory_iterator( directory ) ) |
            std::views::filter( []( auto& e ) { return e.is_regular_file(); } ) ) {
            if ( entry.path().extension() != EXT ) {
                continue;
            }

            std::smatch match{};
            const std::string filename{ entry.path().filename().string() };
            if ( std::regex_match( filename, match, pattern ) && match[1].str() == today ) {
                maxIndex = std::max( maxIndex, std::stoi( match[2].str() ) );
            }
        }

        return string::ToEA_Stl( (directory / fmt::format( "{}-{}-{}{}", BASE, today, maxIndex + 1, EXT )).string() );
    }

    Logger::Logger()
        : mStdOut{ nullptr } {
        Init();
    }

    auto Logger::Init() -> void {

        mStdOut = spdlog::stdout_color_mt( "MIKOTO_STDOUT_LOGGER" );
        mStdErr = spdlog::stderr_color_mt( "MIKOTO_STDERR_LOGGER" );

        constexpr eastl::string_view logsPath{ "Logs" };
        if (filesystem::CreateIfNotExistsDirectory( logsPath )) {
            MKT_CORE_LOGGER_DEBUG( "Created directory for logs" );
        }

        mFileLogName = NextLogFilePath( logsPath.data() );

        try {
            mFile = spdlog::basic_logger_mt( "MIKOTO_FILE_LOGGER", mFileLogName.c_str() );
        } catch ( spdlog::spdlog_ex& e ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to initialize logger: {}", e.what() ) );
        }

        // Set m_CoreLogger pattern.
        // Check out the wiki for info about formatting
        // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
        mStdOut->set_pattern( "%^[%X] STDOUT LOG [thread %t] %v%$" );
        mStdErr->set_pattern( "%^[%X] STDERR LOG [thread %t] %v%$" );
        mFile->set_pattern( "%^[%X] [thread %t] %v%$" );

        // Log every message from the current level onwards.
        // If trace is used, all messages are logged including critical ones,
        // if debug is used, trace messages aren't logged and so on.
        mStdOut->set_level( spdlog::level::trace );
        mStdErr->set_level( spdlog::level::trace );
        mFile->set_level( spdlog::level::trace );

        // Auto flush when "debug" or higher level message is logged on all loggers.
        // Check the FAQ for more about this matter.
        // https://github.com/gabime/spdlog/wiki/0.-FAQ

        // These may be slow, make configurable externally
        mStdOut->flush_on(spdlog::level::trace);
        mStdErr->flush_on(spdlog::level::trace);
        mFile->flush_on(spdlog::level::trace);
    }

    auto Logger::GetStdOutLog() -> const std::shared_ptr<spdlog::logger>& {
        MKT_ASSERT( mStdOut, "STD Out Logger is NULL" );
        return mStdOut;
    }

    auto Logger::GetStdErrLog() -> const std::shared_ptr<spdlog::logger>& {
        MKT_ASSERT( mStdErr, "STD Err Logger is NULL" );
        return mStdErr;
    }

    auto Logger::GetStdFileLog() -> const std::shared_ptr<spdlog::logger>& {
        MKT_ASSERT( mFile, "File Logger is NULL" );
        return mFile;
    }
}// namespace Mikoto