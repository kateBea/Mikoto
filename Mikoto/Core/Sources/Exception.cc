#include <Core/Exception.hh>

namespace mikoto::core {
    auto Exception::GetReason() const noexcept -> const eastl::string& {
        return mMessage;
    }

    RuntimeException::RuntimeException( eastl::string msg )
        : mMessage( eastl::move( msg ) ) {
    }


    RuntimeException::RuntimeException( const char* msg )
        : mMessage( msg ? msg : "" ) {
    }


    auto RuntimeException::what() const noexcept -> const char* {
        MKT_BEGIN_PROFILER();
        return mMessage.c_str();
    }


    auto RuntimeException::Message() const noexcept -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mMessage;
    }
}
