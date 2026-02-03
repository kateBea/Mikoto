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

#ifndef MIKOTO_SHADER_RESOURCE_GROUP_HH
#define MIKOTO_SHADER_RESOURCE_GROUP_HH

#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>
#include <Renderer/Core/FramePassResource.hh>

namespace  Mikoto {

    enum class SRGType {
        SRG_Textures, // Global list of textures visible to all passes
        SRG_PerFrame, // Block of data that can be shared between passes
        SRG_PerPass, // Block of data unique to a pass
        SRG_Constants, // Small data that can be sent per draw call
    };

    // return true if lhs == rhs
    constexpr auto IsSRGType(SRGType lhs, SRGType rhs) -> bool {
        return lhs == rhs;
    }

    class SRGBase {
    public:
        virtual ~SRGBase() = default;

        MKT_NODISCARD auto GetType() const -> SRGType { return m_Type; }

        MKT_NODISCARD auto IsDirty() const -> bool { return m_IsDirty; }

        auto MarkDirty() -> void { m_IsDirty = true; }
        auto ClearDirty() -> void { m_IsDirty = false; }

    protected:
        explicit SRGBase( const SRGType type) : m_Type{ type } {}

        SRGType m_Type{};

        bool m_IsDirty{ true };
    };

    class SRGTextures : public SRGBase {
    public:
        static constexpr Int32 INVALID_TEXTURE_INDEX{ -1 };

    public:

        explicit SRGTextures() : SRGBase{ SRGType::SRG_Textures } {}

        MKT_NODISCARD auto Bind(TextureHandle texture, SamplerHandle sampler) -> Int32;
        MKT_NODISCARD auto Contains(TextureHandle texture, SamplerHandle sampler) -> bool;
        MKT_NODISCARD auto GetIndex(TextureHandle texture, SamplerHandle sampler) -> Int32;

        auto begin() -> decltype(auto) { return m_Resources.begin(); }
        auto end() -> decltype(auto) { return m_Resources.end(); }

        auto cbegin() const -> decltype(auto) { return m_Resources.cbegin(); }
        auto cend() const -> decltype(auto) { return m_Resources.cend(); }

        MKT_NODISCARD static auto GetMaxTextureCount() -> UInt32;

    private:
        ankerl::unordered_dense::map<std::pair<Texture*, Sampler*>, Size> m_Resources{};
    };

    class SRGPerPass : public SRGBase {
    public:
        explicit SRGPerPass() : SRGBase{ SRGType::SRG_PerPass } {}

        auto SetBuffer(std::string_view name, UInt32 binding) -> void;
        auto SetTexture(std::string_view textureName, std::string_view samplerName, UInt32 binding) -> void;

        auto begin() -> decltype(auto) { return m_Resources.begin(); }
        auto end() -> decltype(auto) { return m_Resources.end(); }

        auto cbegin() const -> decltype(auto) { return m_Resources.cbegin(); }
        auto cend() const -> decltype(auto) { return m_Resources.cend(); }

        MKT_NODISCARD auto IsEmpty() const -> bool { return m_Resources.empty(); }

    private:
        struct Entry {
            std::string Name{};
            std::string SamplerName{};

            UInt32 Binding{};
            ShaderResourceType Type{};
        };

        ankerl::unordered_dense::map<std::string, Entry> m_Resources{};

    };

    class SRGPerFrame : public SRGBase {
    public:

        explicit SRGPerFrame() : SRGBase{ SRGType::SRG_PerFrame } {}

    };
}

#endif // MIKOTO_SHADER_RESOURCE_GROUP_HH