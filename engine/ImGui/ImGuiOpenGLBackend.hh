/**
 * ImGuiOpenGLBackend.hh
 * Created by kate on 9/14/23.
 * */


#ifndef MIKOTO_IMGUI_OPENGL_BACKEND_HH
#define MIKOTO_IMGUI_OPENGL_BACKEND_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <vector>

// Third-Party Libraries

// Project Headers
#include <ImGui/ImGuiUtils.hh>

namespace Mikoto {
    class ImGuiOpenGLBackend : public BackendImplementation {
    public:
        auto Init(std::any windowHandle) -> void override;
        auto ShutDown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;
    };
}


#endif // MIKOTO_IMGUI_OPENGL_BACKEND_HH
