/**
 * Logger.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
#include <fmt/chrono.h>
#include <fmt/format.h>

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
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

// This is not within Mikoto but rather fr this specific file
namespace fs = std::filesystem;

namespace Mikoto {

    // Global logger instance
    Logger s_Logger{};

    MKT_NODISCARD static auto FindLatestLogFile( const Path& directory ) -> std::optional<fs::path> {
        // std::filesystem does not guarantee file order, just look for log name with max log index

        const std::regex pattern{ R"(^mikoto-(\d{8})-(\d+)\.log$)" };

        std::optional<fs::path> latestFile{};
        int maxIndex{ -1 };

        // Get today’s date
        const std::string today{ fmt::format( "{:%Y%m%d}", std::chrono::system_clock::now() ) };

        for ( const auto& entry: fs::directory_iterator{ directory } ) {
            if ( !entry.is_regular_file() || entry.path().extension() != ".log" )
                continue;

            const std::string filename = entry.path().filename().string();

            if ( std::smatch match{}; std::regex_match( filename, match, pattern ) ) {
                const std::string& date{ match[1].str() };
                if ( date != today )
                    continue;// ignore logs from other days

                const int index{ std::stoi( match[2].str() ) };
                if ( index > maxIndex ) {
                    maxIndex = index;
                    latestFile = entry.path();
                }
            }
        }

        return latestFile;
    }

    MKT_NODISCARD static auto FormatFileLoggerName( const std::string& existingName ) -> std::string {
        // in a way such that imagine every day we can run the app he first time we do in the day the name is mikoto - 20250114 - 1.log
        // the second time the same day it is mikoto - 20250114 - 2.log third time mikoto - 20250114 - 3.log etc

        constexpr std::string_view EXTENSION{ ".log" };
        constexpr std::string_view SEPARATOR{ "-" };
        constexpr std::string_view BASE{ "mikoto" };

        // Get current date
        const std::string date{ fmt::format( "{:%Y%m%d}", std::chrono::system_clock::now() ) };

        Int32 count{ 1 };// default start
        if ( !existingName.empty() ) {
            const std::regex pattern{ R"(^mikoto-(\d{8})-(\d+)\.log$)" };
            std::smatch match{};
            if ( std::regex_match( existingName, match, pattern ) ) {
                const std::string& existingDate{ match[1].str() };
                const int existingIndex{ std::stoi( match[2].str() ) };
                if ( existingDate == date )
                    count = existingIndex + 1;
            }
        }

        const Int32 index{ count };
        return fmt::format( "{}{}{}{}{}{}", BASE, SEPARATOR, date, SEPARATOR, index, EXTENSION );
    }

    Logger::Logger()
        : m_StdOut{ nullptr } {
        Init();
    }

    auto Logger::Init() -> void {

        m_StdOut = spdlog::stdout_color_mt( "MIKOTO_STDOUT_LOGGER" );
        m_StdErr = spdlog::stderr_color_mt( "MIKOTO_STDERR_LOGGER" );

        auto lastLog{ FindLatestLogFile( fs::current_path() ) };
        if ( lastLog.has_value() ) {
            m_FileLogName = FormatFileLoggerName( lastLog.value().filename() );
        } else {
            m_FileLogName = FormatFileLoggerName( "" );
        }


        // If the file exists create a new one we would not want to miss logs because they get overwritten
        m_FileLogName = FormatFileLoggerName( m_FileLogName );

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
        spdlog::flush_on( spdlog::level::debug );
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