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

#include <Core/Exception.hh>

#include <Assets/AssetFont.hh>

#include <Renderer/Text/Font.hh>

namespace mikoto::asset {

    auto FontLoadDescription::SetFile( filesystem::FileHandle file ) -> FontLoadDescription& {
        MKT_BEGIN_PROFILER();
        mFile = file;
        mPath = mFile ? mFile->GetPath() : filesystem::Path{};
        return *this;
    }

    auto FontLoadDescription::SetSize( float pixelSize ) -> FontLoadDescription& {
        MKT_BEGIN_PROFILER();
        mSize = pixelSize;
        return *this;
    }


    Font::Font( core::Ref<renderer::Font> resource )
        : mResource{ eastl::move( resource ) } {
        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot construct Font without a resource." );
        }
    }

    Font::~Font() = default;
    auto Font::GetResource() const -> core::Ref<renderer::Font> {
        return mResource;
    }

    auto Font::CanEvict() const -> bool {
        return mResource->GetRefCount() == 1;
    }

    auto Font::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return { sizeof( *this ), 0 };
    }
}// namespace mikoto::asset
