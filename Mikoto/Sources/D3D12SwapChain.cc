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

#include <Core/Platform.hh>

#include <Renderer/D3D12/D3D12SwapChain.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

namespace mikoto::renderer::d3d12 {

    SwapChain::SwapChain( Window *window, Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory )
        : mWindow{ window }, mDxgiFactory{ mDxgiFactory } {
        MKT_ASSERT( mWindow, "Window handle cannot be NULL." );

        mWidth = mWindow->GetWidth();
        mHeight = mWindow->GetHeight();
    }

    auto SwapChain::Present() -> void {

    }

    auto SwapChain::OnResize( u32 width, u32 height ) -> void {
        mWidth = width;
        mHeight = height;
    }

    auto SwapChain::SetRefreshRate( RefreshRate type ) -> void {
        mRefreshRate = type;
    }

    auto SwapChain::GetWidth() const -> u32 {
        return mWidth;
    }

    auto SwapChain::GetHeight() const -> u32 {
        return mHeight;
    }

    auto SwapChain::GetFormat() const -> Format {
        return mFormat;
    }

    auto SwapChain::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            default:;
        }

        return Object(nullptr);
    }

    auto SwapChain::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            default:;
        }

        return Object(nullptr);
    }

    SwapChain::~SwapChain() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto SwapChain::Initialize() -> void {
        mIsAllocated = true;
    }

    auto SwapChain::Release() -> void {

    }
}// namespace mikoto::renderer::d3d12

#endif