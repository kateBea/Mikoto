#ifndef MIKOTO_RUNTIME_EXCEPTION_HH
#define MIKOTO_RUNTIME_EXCEPTION_HH

#include <exception>
#include <string>
#include <utility>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    class RuntimeException : public std::exception {
    public:
        // Construct from std::string
        explicit RuntimeException( std::string msg )
            : m_Message( std::move( msg ) ) {}

        // Construct from const char*
        explicit RuntimeException( const char* msg )
            : m_Message( msg ? msg : "" ) {}

        // Default copy and move support
        RuntimeException( const RuntimeException& ) = default;
        RuntimeException& operator=( const RuntimeException& ) = default;
        RuntimeException( RuntimeException&& ) noexcept = default;
        RuntimeException& operator=( RuntimeException&& ) noexcept = default;

        auto what() const noexcept -> const char* override {
            return m_Message.c_str();
        }

        auto Message() const noexcept -> const std::string& {
            return m_Message;
        }

    private:
        std::string m_Message;
    };
}

#endif // MIKOTO_RUNTIME_EXCEPTION_HH
