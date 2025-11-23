/**
 * InputService.hh
 * Created by kate on 5/30/23.
 * */

#ifndef MIKOTO_INPUT_MANAGER_HH
#define MIKOTO_INPUT_MANAGER_HH

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window.hh>
#include <Common/Singleton.hh>

namespace Mikoto {

    enum class CursorInputMode {
        CURSOR_NORMAL,
        CURSOR_HIDDEN,
        CURSOR_DISABLED,
    };

    struct InputServiceCreateInfo {
        Window* MainWindow{ nullptr };
    };

    class InputService final : public IService, public Singleton<InputService> {
    public:
        explicit InputService( const InputServiceCreateInfo& options );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt ) -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool;

        MKT_NODISCARD auto GetMouseX() const -> double;
        MKT_NODISCARD auto GetMouseY() const -> double;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double>;

        auto SetFocus( Window* newHandle ) -> void;
        auto SetCursorMode( CursorInputMode mode ) const -> void;

        ~InputService() override = default;

    private:
        Window* m_Handle{ nullptr };
    };
}

#endif// MIKOTO_INPUT_MANAGER_HH