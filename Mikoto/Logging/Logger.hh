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

#ifndef MIKOTO_LOGGING_LOGGER_HH
#define MIKOTO_LOGGING_LOGGER_HH

#include <mutex>
#include <memory>

#include <EASTL/string.h>
#include <EASTL/functional.h>

#include <spdlog/spdlog.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>
#include <Core/Exception.hh>

namespace mikoto::logging {

    /** @brief Supported logging severity levels, mapping directly to runtime filtering thresholds. */
    enum class Severity {
        eTrace,    ///< Ultra-verbose diagnostics.
        eDebug,    ///< Operational diagnostics useful during development.
        eInfo,     ///< General informational messages emphasizing system milestones.
        eWarning,  ///< Non-critical anomalies or unexpected behaviors.
        eError,    ///< Recoverable operational errors or failures.
        eCritical, ///< Catastrophic, unrecoverable system failures.
    };

    /** @brief Defines the physical target mediums where logged data can be dispatched. */
    enum class Output {
        eConsole, ///< Standard output streams (stdout/stderr) with ANSI color terminal support.
        eFile,    ///< Persistent storage file logging.
    };

    class ILogger : public core::IService, public core::Singleton<ILogger> {
    public:
        /** @brief Dispatches a raw text message at the specified severity level.
         *
         * @param severity The destination urgency tier.
         * @param message The raw string text to append to the log.
         */
        virtual auto Log( Severity severity, eastl::string_view message ) -> void = 0;


        /** @brief Dispatches structured exception metadata at the specified severity level.
         *
         * @param severity The destination urgency tier.
         * @param exception The core exception instance to extract data from.
         */
        virtual auto Log( Severity severity, const core::Exception& exception ) -> void = 0;


        /** @brief Logs a raw text message at the Trace level.
         *
         * @param message The raw string text to append to the log.
         */
        virtual auto LogTrace( eastl::string_view message ) -> void = 0;


        /** @brief Logs a raw text message at the Debug level.
         *
         * @param message The raw string text to append to the log.
         */
        virtual auto LogDebug( eastl::string_view message ) -> void = 0;


        /** @brief Logs a raw text message at the Error level.
         *
         * @param message The raw string text to append to the log.
         */
        virtual auto LogError( eastl::string_view message ) -> void = 0;


        /** @brief Logs a raw text message at the Warning level.
         *
         * @param message The raw string text to append to the log.
         */
        virtual auto LogWarning( eastl::string_view message ) -> void = 0;


        /** @brief Logs a raw text message at the Critical level.
         *
         * @param message The raw string text to append to the log.
         */
        virtual auto LogCritical( eastl::string_view message ) -> void = 0;


        /** @brief Logs a raw text message at the Information level.
         *
         * @param message The raw string text to append to the log.
         */
        virtual auto LogInformation( eastl::string_view message ) -> void = 0;


        /** @brief Logs structured exception metadata at the Trace level.
         *
         * @param exception The core exception instance to extract data from.
         */
        virtual auto LogTrace( const core::Exception& exception ) -> void = 0;


        /** @brief Logs structured exception metadata at the Debug level.
         *
         * @param exception The core exception instance to extract data from.
         */
        virtual auto LogDebug( const core::Exception& exception ) -> void = 0;


        /** @brief Logs structured exception metadata at the Error level.
         *
         * @param exception The core exception instance to extract data from.
         */
        virtual auto LogError( const core::Exception& exception ) -> void = 0;


        /** @brief Logs structured exception metadata at the Warning level.
         *
         * @param exception The core exception instance to extract data from.
         */
        virtual auto LogWarning( const core::Exception& exception ) -> void = 0;


        /** @brief Logs structured exception metadata at the Critical level.
         *
         * @param exception The core exception instance to extract data from.
         */
        virtual auto LogCritical( const core::Exception& exception ) -> void = 0;


        /** @brief Logs structured exception metadata at the Information level.
         *
         * @param exception The core exception instance to extract data from.
         */
        virtual auto LogInformation( const core::Exception& exception ) -> void = 0;


        /** @brief Restricts output by discarding logs carrying a lower severity level.
         *
         * @param severity The minimum inclusive severity threshold to process.
         */
        virtual auto SetLevel( Severity severity ) -> void = 0;


        /** @brief Updates the structural output formatting layout across all active targets.
         *
         * @param pattern The spdlog-compliant token format layout string.
         * @deprecated I think this will be moved to concrete classes, because this is tied to specific implementation
         *      if someone uses an interface it is because they do not care about the underlying interface.
         */
        virtual auto SetPattern( eastl::string_view pattern ) -> void = 0;


        /** @brief Manually forces immediate block-synchronization of unwritten buffers to underlying sinks. */
        virtual auto Flush() -> void = 0;


        /** @brief Configures automated flush operations when a message meets or exceeds a specific threshold.
         *
         * @param severity The inclusive lower-bound threshold triggering an immediate physical flush.
         */
        virtual auto FlushOn( Severity severity ) -> void = 0;


        /** @brief Configures or redirects the primary target sink subsystem destination.
         *
         * @param output The hardware or subsystem target medium type.
         * @param path Target filepath destination; required for file outputs, ignored for consoles.
         */
        virtual auto SetLogOutput( Output output, eastl::string_view path ) -> void = 0;


        /** @brief Returns the active minimum inclusive logging threshold.
         *
         * @return The current severity filter level.
         */
        MKT_NODISCARD virtual auto GetLevel() const -> Severity = 0;


        /** @brief Returns the minimum logging tier configured to trigger an automated sink flush.
         *
         * @return The current severity flush threshold level.
         */
        MKT_NODISCARD virtual auto GetFlushLevel() const -> Severity = 0;


        /** @brief Returns the unique identifying name of this logger instance.
         *
         * @return A view of the underlying logger instance name.
         */
        MKT_NODISCARD virtual auto GetName() const -> eastl::string_view = 0;


        /** @brief Evaluates whether a log statement will pass the active level filter.
         *
         * @param severity The tier level to test against the active filter.
         * @return True if the level meets or exceeds the active threshold, otherwise false.
         */
        MKT_NODISCARD virtual auto ShouldLog( Severity severity ) const -> bool = 0;
    };

    class DefaultLogger final : public ILogger {
    public:
        explicit DefaultLogger();

        // Concrete access is needed only for the console observer and legacy file logging.
        MKT_NODISCARD static auto GetPtr() -> DefaultLogger*;

        auto GetStdOutLog() const -> std::shared_ptr<spdlog::logger>;
        auto GetStdErrLog() const -> std::shared_ptr<spdlog::logger>;
        auto GetStdFileLog() const -> std::shared_ptr<spdlog::logger>;

        // Observer must not log recursively. Clearing waits for callbacks.
        auto SetConsoleObserver( eastl::function<void( Severity, eastl::string_view )> observer ) -> void;

        auto Log( Severity severity, eastl::string_view message ) -> void override;
        auto Log( Severity severity, const core::Exception& exception ) -> void override;

        auto LogTrace( eastl::string_view message ) -> void override;
        auto LogDebug( eastl::string_view message ) -> void override;
        auto LogError( eastl::string_view message ) -> void override;
        auto LogWarning( eastl::string_view message ) -> void override;
        auto LogCritical( eastl::string_view message ) -> void override;
        auto LogInformation( eastl::string_view message ) -> void override;

        auto LogTrace( const core::Exception& exception ) -> void override;
        auto LogDebug( const core::Exception& exception ) -> void override;
        auto LogError( const core::Exception& exception ) -> void override;
        auto LogWarning( const core::Exception& exception ) -> void override;
        auto LogCritical( const core::Exception& exception ) -> void override;
        auto LogInformation( const core::Exception& exception ) -> void override;

        auto SetLevel( Severity severity ) -> void override;
        auto SetPattern( eastl::string_view pattern ) -> void override;
        auto SetLogOutput( Output output, eastl::string_view path ) -> void override;

        auto Flush() -> void override;
        auto FlushOn( Severity severity ) -> void override;

        MKT_NODISCARD auto GetLevel() const -> Severity override;
        MKT_NODISCARD auto GetFlushLevel() const -> Severity override;
        MKT_NODISCARD auto GetName() const -> eastl::string_view override;
        MKT_NODISCARD auto ShouldLog( Severity severity ) const -> bool override;

        ~DefaultLogger() override;

    private:
        auto Initialize() -> void override;
        auto Shutdown() -> void override;

    private:
        // spdlog uses std::shared_ptr
        std::shared_ptr<spdlog::logger> mStdOut{};
        std::shared_ptr<spdlog::logger> mStdErr{};
        std::shared_ptr<spdlog::logger> mFile{};

        eastl::string mFileLogName{ "" };

        mutable std::mutex mStateMutex{};
        Output mOutput{ Output::eConsole };
        Severity mLoggingSeverity{ Severity::eTrace };
        Severity mFlushSeverity{ Severity::eTrace };
        eastl::string mPattern{ "%^[%X] [thread %t] %v%$" };

        mutable std::mutex mConsoleObserverMutex{};
        eastl::function<void( Severity, eastl::string_view )> mConsoleObserver{};
    };

}

#if !defined(NDEBUG)
    #define MKT_ENABLE_LOGGING
#else
    #undef MKT_ENABLE_LOGGING
#endif

#if defined(MKT_ENABLE_LOGGING)
    #define MKT_CORE_LOGGER_ERROR(...) mikoto::logging::ILogger::Get().LogError(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_WARN(...) mikoto::logging::ILogger::Get().LogWarning(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_CRITICAL(...) mikoto::logging::ILogger::Get().LogCritical(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_TRACE(...) mikoto::logging::ILogger::Get().LogTrace(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_INFO(...) mikoto::logging::ILogger::Get().LogInformation(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_DEBUG(...) mikoto::logging::ILogger::Get().LogDebug(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))

    #define MKT_FILE_LOGGER_ERROR(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_WARN(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->warn(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_CRITICAL(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_TRACE(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->trace(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_INFO(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->info(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_DEBUG(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->debug(fmt::format(__VA_ARGS__))

    #define MKT_PRINT_STACK_TRACE() cpptrace::generate_trace().print()
#else
    // I still want to see errors and critical logs
    #define MKT_CORE_LOGGER_ERROR(...) mikoto::logging::ILogger::Get().LogError(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_CRITICAL(...) mikoto::logging::ILogger::Get().LogCritical(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))
    #define MKT_CORE_LOGGER_INFO(...) mikoto::logging::ILogger::Get().LogInformation(mikoto::string::ConvertToEastl(fmt::format(__VA_ARGS__)))

    #define MKT_CORE_LOGGER_TRACE(...)
    #define MKT_CORE_LOGGER_WARN(...)
    #define MKT_CORE_LOGGER_DEBUG(...)

    #define MKT_FILE_LOGGER_CRITICAL(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_ERROR(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_INFO(...) mikoto::logging::DefaultLogger::GetPtr()->GetStdFileLog()->info(fmt::format(__VA_ARGS__))

    #define MKT_FILE_LOGGER_WARN(...)
    #define MKT_FILE_LOGGER_TRACE(...)
    #define MKT_FILE_LOGGER_DEBUG(...)

    #define MKT_STACK_TRACE() cpptrace::generate_trace().print()
#endif

namespace mikoto::logging::stacktrace {

    MKT_NODISCARD auto ToString() -> eastl::string;
}// namespace mikoto

#endif// MIKOTO_LOGGING_LOGGER_HH
