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

#include <ImGui/ImGuiD3D12Backend.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <imgui_impl_dx12.h>

namespace Mikoto {

    auto ImGuiD3D12Backend::Init() -> void {

    }

    auto ImGuiD3D12Backend::Shutdown() -> void {

    }

    auto ImGuiD3D12Backend::BeginFrame() -> void {

    }

    auto ImGuiD3D12Backend::EndFrame() -> void {

    }

    auto ImGuiD3D12Backend::GetFinalComposition() -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto ImGuiD3D12Backend::ConstructImGuiTextureID( const Texture *texture ) -> ImTextureID {
        return 0;
    }

    auto ImGuiD3D12Backend::ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID {
        return 0;
    }
}

#endif