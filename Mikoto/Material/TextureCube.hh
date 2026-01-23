//    Copyright 2025 ケイト
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

#ifndef MIKOTO_TEXTURE_CUBE_MAP_HH
#define MIKOTO_TEXTURE_CUBE_MAP_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Assets/Texture.hh>

namespace Mikoto {

    class TextureCube : public Texture {
    public:

        MKT_NODISCARD auto IsHDR() const -> bool { return m_IsHDR; }
        MKT_NODISCARD auto GetMipLevels() const -> UInt32 { return m_MipLevels; }

        ~TextureCube() override = default;

    protected:
        static constexpr UInt32 MAX_CUBE_MAP_FACES{ 6 };

        explicit TextureCube(const ResourceUsageType usage, const UInt32 mipLevels = 1)
            : Texture{ TextureType::TEXTURE_CUBE, TextureFormat::RGBA8_SNORM,
                0, 0, 0, usage, TextureUsage::CUBE }, m_MipLevels{ mipLevels }
        {}

        UInt32 m_MipLevels{ 1 };
        bool m_IsHDR{ false };
    };
}// namespace Mikoto

#endif//MIKOTO_TEXTURE_CUBE_MAP_HH
