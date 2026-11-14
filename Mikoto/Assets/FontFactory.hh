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

#ifndef MIKOTO_ASSETS_FONT_FACTORY_HH
#define MIKOTO_ASSETS_FONT_FACTORY_HH

#include <mutex>

#include <EASTL/span.h>

#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/File.hh>

#include <Assets/AssetFont.hh>

#include <Text/FontAtlas.hh>

#include <Renderer/Text/Font.hh>

namespace mikoto::asset {

    struct FontFactoryCreateInfo {
        renderer::rhi::IGpuDevice* mDevice{};
    };

    class FontFactory final : public core::IService, public core::Singleton<FontFactory> {
    public:
        explicit FontFactory( const FontFactoryCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto LoadFont( const FontLoadDescription& description ) -> renderer::FontHandle;
        auto EnsureGlyphs( renderer::Font& font, eastl::span<const core::u32> codepoints ) -> void;

    private:
        auto EnsureColorGlyphs( renderer::Font& font, eastl::span<const core::u32> codepoints ) -> void;
        auto UploadAtlas( const renderer::FontAtlasData& data ) -> renderer::rhi::TextureHandle;
        auto UploadSlugAtlas( const renderer::SlugHornAtlasData& data ) -> renderer::SlugTexturePage;

    private:
        renderer::rhi::IGpuDevice* mDevice{};
        std::mutex mGlyphMutex{};
    };
}// namespace mikoto::asset

#endif// MIKOTO_ASSETS_FONT_FACTORY_HH

