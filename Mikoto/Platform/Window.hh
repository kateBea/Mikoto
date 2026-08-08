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

#ifndef MIKOTO_WINDOW_HH
#define MIKOTO_WINDOW_HH

#include <EASTL/any.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>
#include <Core/Types.hh>
#include <Renderer/Rhi/Types.hh>

namespace mikoto::platform {

    using namespace core;
    using namespace renderer;
    using namespace renderer::rhi;

    enum class ScreenMode {
        eFullScreen,
        eWindowed,
        eBorderless,
    };

    enum class CursorMode {
        eNormal,
        eHidden,
        eDisabled
    };

    enum class CursorType {
        eArrow,
        eHand,
        eText,
        eCrossHair,
        eResizeHorizontal,
        eResizeVertical
    };

    struct WindowProperties {
        eastl::string mTitle{};

        i32 mWidth{};
        i32 mHeight{};

        GraphicsAPI mBackend{};

        bool mResizable{ false };
    };

    class Window {
    public:
        explicit Window( const WindowProperties& props = WindowProperties{} )
            : mTitle{ props.mTitle },
              mWidth{ props.mWidth },
              mHeight{ props.mHeight },
              mBackend{ props.mBackend },
              mIsResizable{ props.mResizable } {}

        virtual auto SetWidth( i32 width ) -> void = 0;
        virtual auto SetHeight( i32 height ) -> void = 0;
        virtual auto SetTitle( eastl::string_view title ) -> void = 0;

        MKT_NODISCARD auto GetWidth() const -> i32 { return mWidth; }
        MKT_NODISCARD auto GetHeight() const -> i32 { return mHeight; }
        MKT_NODISCARD auto GetTitle() const -> const eastl::string& { return mTitle; }
        MKT_NODISCARD auto GetScreenMode() const -> ScreenMode { return mScreenMode; }

        MKT_NODISCARD auto IsMinimized() const -> bool { return GetWidth() == 0 || GetHeight() == 0; }
        MKT_NODISCARD auto IsMaximized() const -> bool { return mScreenMode == ScreenMode::eFullScreen; }

        MKT_NODISCARD auto IsResizable() const -> bool { return mIsResizable; }
        MKT_NODISCARD auto IsApi( GraphicsAPI api ) const -> bool { return mBackend == api; }
        MKT_NODISCARD auto GetApi() const -> GraphicsAPI { return mBackend; }

        auto AllowResizing( const bool value ) -> void { mIsResizable = value; }

        virtual auto SetScreenMode( ScreenMode mode ) -> void = 0;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto ProcessEvents() -> void = 0;

        MKT_NODISCARD virtual auto GetNativeWindow() const -> eastl::any = 0;

        MKT_NODISCARD virtual auto IsKeyPressed( KeyCode keyCode ) const -> bool = 0;
        MKT_NODISCARD virtual auto IsKeyReleased( KeyCode keyCode ) const -> bool = 0;

        MKT_NODISCARD virtual auto IsMouseKeyPressed( MouseButton button ) const -> bool = 0;
        MKT_NODISCARD virtual auto IsMouseKeyReleased( MouseButton button ) const -> bool = 0;

        MKT_NODISCARD virtual auto GetMouseX() const -> f64 = 0;
        MKT_NODISCARD virtual auto GetMouseY() const -> f64 = 0;
        MKT_NODISCARD virtual auto GetMousePos() const -> std::pair<f64, f64> = 0;

        MKT_NODISCARD virtual auto ShouldClose() const -> bool = 0;

        MKT_NODISCARD auto IsCursorMode( const CursorMode mode ) const -> bool { return mCursorMode == mode; }
        MKT_NODISCARD auto IsCursorType( const CursorType type ) const -> bool { return mCursorType == type; }

        MKT_NODISCARD auto GetCursorMode() const -> CursorMode { return mCursorMode; }
        MKT_NODISCARD auto GetCursorType() const -> CursorType { return mCursorType; }

        virtual auto SetCursorMode( CursorMode mode ) -> void = 0;
        virtual auto SetCursorType( CursorType type ) -> void = 0;

        virtual auto ResetCursorType() -> void = 0;

        virtual ~Window() = default;

    public:
        DISABLE_COPY_FOR( Window );

    protected:
        eastl::string mTitle{};

        i32 mWidth{};
        i32 mHeight{};

        GraphicsAPI mBackend{};

        CursorMode mCursorMode{ CursorMode::eNormal };
        CursorType mCursorType{ CursorType::eArrow };

        ScreenMode mScreenMode{ ScreenMode::eWindowed };

        bool mIsResizable{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_WINDOW_HH
