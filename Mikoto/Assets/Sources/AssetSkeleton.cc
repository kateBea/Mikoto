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

#include <Assets/AssetSkeleton.hh>

namespace mikoto::asset {
    Skeleton::Skeleton( ModelHandle model, const SkeletonLoadDescription& description )
        : mModel{ eastl::move( model ) } {
        if ( !mModel ) {
            MKT_THROW_RUNTIME_ERROR( "Skeleton requires a loaded model." );
        }

        mResource = mModel->GetSkeleton( description.mRigIndex );

        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Requested skeleton is missing from the model." );
        }
    }

    auto Skeleton::GetResource() const -> const animation::Skeleton* {
        return mResource;
    }

    auto Skeleton::GetModel() const -> ModelHandle {
        return mModel;
    }

    auto Skeleton::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        // Shared geometry/rig memory is counted once by the model registry.
        return { sizeof( *this ), 0 };
    }
}// namespace mikoto::asset
