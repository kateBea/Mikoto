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

#ifndef MIKOTO_ASSETS_ASSET_ANIMATION_HH
#define MIKOTO_ASSETS_ASSET_ANIMATION_HH

#include <Assets/AssetModel.hh>

namespace mikoto::asset {

    struct AnimationLoadDescription {
        filesystem::Path mPath{};
        eastl::string mName{};
        core::u32 mRigIndex{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    /** Retains the imported model that owns this immutable sub-asset. */
    class Animation final : public core::ReferenceCounted<Animation> {
    public:
        Animation( ModelHandle model, const AnimationLoadDescription& description );
        MKT_NODISCARD auto GetResource() const -> const animation::SkinnedAnimation*;
        MKT_NODISCARD auto GetModel() const -> ModelHandle;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;
    private:
        ModelHandle mModel{};
        const animation::SkinnedAnimation* mResource{};
    };

    template<>
    struct AssetTraits<Animation> {
        using Asset = Animation;
        using Description = AnimationLoadDescription;

        static constexpr AssetType kType{ AssetType::eAnimation };
    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_ANIMATION_HH
