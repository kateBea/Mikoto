//
// Created by zanet on 12/23/2025.
//

#ifndef MIKOTO_SHADERRESOURCEGROUP_HH
#define MIKOTO_SHADERRESOURCEGROUP_HH


#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameResource.hh>

namespace  Mikoto {

    enum class SRGType {
        SRG_Textures,
        SRG_PerFrame,
        SRG_PerPass
    };

    // return true if lhs == rhs
    constexpr auto IsSRGType(SRGType lhs, SRGType rhs) -> bool {
        return lhs == rhs;
    }

    class SRGBase {
    public:
        virtual ~SRGBase() = default;

        MKT_NODISCARD auto GetType() const -> SRGType { return m_Type; }

    protected:
        explicit SRGBase( const SRGType type) : m_Type{ type } {}

        SRGType m_Type{};
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

        MKT_NODISCARD static auto GetMaxBindlessTextureCount() -> UInt32;

    private:
        ankerl::unordered_dense::map<std::pair<Texture*, Sampler*>, Size> m_Resources{};
    };

    class SRGPerPass : public SRGBase {
    public:
        explicit SRGPerPass() : SRGBase{ SRGType::SRG_PerPass } {}

        auto SetBuffer(std::string_view name, UInt32 binding, ShaderResourceType type) -> void;

        auto begin() -> decltype(auto) { return m_Resources.begin(); }
        auto end() -> decltype(auto) { return m_Resources.end(); }

        auto cbegin() const -> decltype(auto) { return m_Resources.cbegin(); }
        auto cend() const -> decltype(auto) { return m_Resources.cend(); }

    private:
        struct Entry {
            std::string Name{};
            UInt32 Binding{};
            ShaderResourceType Type{ ShaderResourceType::SHADER_RESOURCE_UNDEFINED };
        };

        std::vector<Entry> m_Resources{};

    };

    class SRGPerFrame : public SRGBase {
    public:

        explicit SRGPerFrame() : SRGBase{ SRGType::SRG_PerFrame } {}

    };
}


#endif //MIKOTO_SHADERRESOURCEGROUP_HH