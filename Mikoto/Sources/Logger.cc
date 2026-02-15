/**
 * Logger.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
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

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// Project Headers
#include <Core/Exception.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

// This is not within Mikoto but rather fr this specific file
namespace fs = std::filesystem;

namespace Mikoto {

    // Global logger instance
    Logger s_Logger{};

    MKT_NODISCARD
    static auto NextLogFilePath( const fs::path& directory ) -> fs::path {
        constexpr std::string_view BASE{ "mikoto" };
        constexpr std::string_view EXT{ ".log" };
        const std::string today{ fmt::format( "{:%Y%m%d}", std::chrono::system_clock::now() ) };

        // Regex pattern to match log files like mikoto-20251014-1.log,
        // mikoto, then date, then log count index
        const std::regex pattern{ R"(mikoto-(\d{8})-(\d+)\.log)" };

        Int32 maxIndex{};

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

        return directory / fmt::format( "{}-{}-{}{}", BASE, today, maxIndex + 1, EXT );
    }

    Logger::Logger()
        : m_StdOut{ nullptr } {
        Init();
    }

    auto Logger::Init() -> void {

        m_StdOut = spdlog::stdout_color_mt( "MIKOTO_STDOUT_LOGGER" );
        m_StdErr = spdlog::stderr_color_mt( "MIKOTO_STDERR_LOGGER" );

        m_FileLogName = NextLogFilePath( fs::current_path() ).string();

        try {
            m_File = spdlog::basic_logger_mt( "MIKOTO_FILE_LOGGER", m_FileLogName );
        } catch ( spdlog::spdlog_ex& e ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to initialize logger: {}", e.what() ) );
        }

        // Set m_CoreLogger pattern.
        // Check out the wiki for info about formatting
        // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
        m_StdOut->set_pattern( "%^[%X] STDOUT LOG [thread %t] %v%$" );
        m_StdErr->set_pattern( "%^[%X] STDERR LOG [thread %t] %v%$" );
        m_File->set_pattern( "%^[%X] [thread %t] %v%$" );

        // Log every message from the current level onwards.
        // If trace is used, all messages are logged including critical ones,
        // if debug is used, trace messages aren't logged and so on.
        m_StdOut->set_level( spdlog::level::trace );
        m_StdErr->set_level( spdlog::level::trace );
        m_File->set_level( spdlog::level::trace );

        // Auto flush when "debug" or higher level message is logged on all loggers.
        // Check the FAQ for more about this matter.
        // https://github.com/gabime/spdlog/wiki/0.-FAQ

        // These may be slow, make configurable externally
        m_StdOut->flush_on(spdlog::level::trace);
        m_StdErr->flush_on(spdlog::level::trace);
        m_File->flush_on(spdlog::level::trace);
    }

    auto Logger::GetStdOutLog() -> const Shared<spdlog::logger>& {
        MKT_ASSERT( m_StdOut, "STD Out Logger is NULL" );
        return m_StdOut;
    }

    auto Logger::GetStdErrLog() -> const Shared<spdlog::logger>& {
        MKT_ASSERT( m_StdErr, "STD Err Logger is NULL" );
        return m_StdErr;
    }

    auto Logger::GetStdFileLog() -> const Shared<spdlog::logger>& {
        MKT_ASSERT( m_File, "File Logger is NULL" );
        return m_File;
    }
}// namespace Mikoto