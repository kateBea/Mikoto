#include <Core/Application.hh>

namespace mikoto::core {
    auto Application::IsRunning() const -> bool {
        MKT_BEGIN_PROFILER();
        return mState == ApplicationStatus::eRunning || mState == ApplicationStatus::eIdle;
    }
}
