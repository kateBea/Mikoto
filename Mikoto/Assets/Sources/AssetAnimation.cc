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

#include <Assets/AssetAnimation.hh>

namespace mikoto::asset {
    Animation::Animation( ModelHandle model, const AnimationLoadDescription& description )
        : mModel{ eastl::move( model ) } {
        if ( !mModel ) {
            MKT_THROW_RUNTIME_ERROR( "Animation requires a loaded model." );
        }

        if ( description.mName.empty() ) {
            const auto& clips{ mModel->GetAnimations( description.mRigIndex ) };

            if ( clips.size() != 1 ) {
                MKT_THROW_RUNTIME_ERROR( "Specify an animation name when the model does not contain exactly one clip." );
            }

            mResource = clips.begin()->second.get();
        }
        else {
            mResource = mModel->FindAnimation( description.mName, description.mRigIndex );
        }

        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Requested animation is missing from the model." );
        }
    }

    auto Animation::GetResource() const -> const animation::SkinnedAnimation* {
        return mResource;
    }

    auto Animation::GetModel() const -> ModelHandle {
        return mModel;
    }

    auto Animation::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        // Shared geometry/rig memory is counted once by the model registry.
        return { sizeof( *this ), 0 };
    }
}// namespace mikoto::asset
