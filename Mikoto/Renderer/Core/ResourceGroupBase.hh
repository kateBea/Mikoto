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

#include <vector>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>
#include <Renderer/Core/FramePassResource.hh>

namespace  Mikoto {

    enum class ResourceGroup {
        UnboundedImageViews,  // For descriptor indexing with images (like bindless textures

        ImageViews, // Storage images (storage image, unused for now, can be used in compute)
        BufferViews,  // Uniform buffers/ Storage (Is updated frequently every frame)
        
        UnorderedAccessViews, // Storage buffer (does not require frequent change, usually changed via copy command)

        StaticSamplers, // It is always the same image
        DynamicSamplers, // Can change sometimes

        UnboundedBufferViews, // For descriptor indexing with buffers

        Constants, // Uniform buffers (does not require frequent change)

        GlobalTextures,
        Dynamic,
        Static,
    };

    enum class ResourceSlot {
        Slot_0,
        Slot_1,
        Slot_2,
        Slot_3,
        Slot_4,
        Slot_5,
        Slot_6,
        Slot_7,
        Slot_8,
        Slot_Max,
    };

    class ResourceGroupBase {
    public:
        virtual ~ResourceGroupBase() = default;

        MKT_NODISCARD auto GetType() const -> ResourceGroup { return m_Type; }

        MKT_NODISCARD auto IsDirty() const -> bool { return m_IsDirty; }

        auto MarkDirty() -> void { m_IsDirty = true; }
        auto ClearDirty() -> void { m_IsDirty = false; }

    protected:
        explicit ResourceGroupBase( const ResourceGroup type) : m_Type{ type } {}

        ResourceGroup m_Type{};

        bool m_IsDirty{ true };
    };

    class GlobalTextures : public ResourceGroupBase {
    public:
        static constexpr Int32 INVALID_TEXTURE_INDEX{ -1 };

    public:

        explicit GlobalTextures() : ResourceGroupBase{ ResourceGroup::GlobalTextures } {}

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

    class ConstantsGroup final : public ResourceGroupBase {
    public:

        explicit ConstantsGroup() : ResourceGroupBase{ ResourceGroup::Constants } {}

        auto SetData(const void* ptr, Size size) -> void;

        auto Clear() -> void;

        MKT_NODISCARD auto GetSize() const -> Size { return m_SizeBytes; }
        MKT_NODISCARD auto GetData() const -> const void* { return m_Data; }
        MKT_NODISCARD auto IsEmpty() const -> Size { return m_Data == nullptr && m_SizeBytes == 0; }

    private:

        const void* m_Data{};
        Size m_SizeBytes{};
    };

    class CommonResourceGroup final : public ResourceGroupBase {
    public:
        explicit CommonResourceGroup( ResourceGroup type = ResourceGroup::Dynamic ) : ResourceGroupBase{ type } {}

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
}

#endif // MIKOTO_SHADER_RESOURCE_GROUP_HH