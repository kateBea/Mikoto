//
// Created by zanet on 4/10/2025.
//

#ifndef LOGGINGSERVICE_HH
#define LOGGINGSERVICE_HH

#include <Common/Service.hh>
#include <Core/Logger.hh>

namespace Mikoto {

    enum class LoggingSeverity {
        LOGGING_SEVERITY_DEBUG = 0,
        LOGGING_SEVERITY_INFO,
        LOGGING_SEVERITY_WARNING,
        LOGGING_SEVERITY_ERROR,
        LOGGING_SEVERITY_CRITICAL,
    };

    struct LoggingServiceDescription {
        Path_T LogFilePath{};
        LoggingSeverity Severity{ LoggingSeverity::LOGGING_SEVERITY_DEBUG };
    };

    class LoggingService final : public IService<LoggingService> {
    public:
        auto Init() -> void override;
        auto Shutdown() -> void override;

    private:

        // Global instance for logging
        Logger m_Logger{};
    };
}// namespace Mikoto


#endif//LOGGINGSERVICE_HH
