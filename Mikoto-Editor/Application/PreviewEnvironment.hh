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

#ifndef MIKOTO_EDITOR_PREVIEW_ENVIRONMENT_HH
#define MIKOTO_EDITOR_PREVIEW_ENVIRONMENT_HH

#include <EASTL/shared_ptr.h>

#include <Assets/AssetTexture.hh>

#include <Filesystem/Path.hh>

#include <Renderer/RHI/Texture.hh>

namespace mikoto::editor {
    /** Loads preview lighting off the UI thread; jobs never capture a panel. */
    class PreviewEnvironment final {
    public:
        static auto StreetPath() -> filesystem::Path;
        MKT_NODISCARD static auto DaylightPath() -> filesystem::Path;
        auto Request( const filesystem::Path &path ) -> void;
        /** Returns a completed texture once. A new request replaces only the mailbox. */
        auto Poll( renderer::rhi::TextureHandle &texture ) -> bool;
        auto Poll( asset::AssetHandle<asset::Texture> &texture ) -> bool;
        MKT_NODISCARD auto IsRequested() const -> bool;
        MKT_NODISCARD auto GetError() const -> const eastl::string &;

    private:
        struct Load;
        eastl::shared_ptr<Load> mLoad{};
        eastl::string mError{};
        bool mConsumed{};
    };
}// namespace mikoto::editor
#endif
