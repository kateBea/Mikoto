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

#include <mutex>

#include <Application/PreviewEnvironment.hh>

#include <Assets/AssetRegistryService.hh>

#include <Core/Exception.hh>

#include <Threading/TaskService.hh>

namespace mikoto::editor {
    struct PreviewEnvironment::Load {
        std::mutex mMutex{};
        asset::AssetHandle<asset::Texture> mTexture{};
        eastl::string mError{};
        bool mComplete{};
    };

    auto PreviewEnvironment::StreetPath() -> filesystem::Path {
        filesystem::Path path{ "Resources/Cubemaps/modern_evening_street_4k.hdr" };
        return path.Exists() ? path : filesystem::Path{ "Mikoto/Resources/Resources/Cubemaps/modern_evening_street_4k.hdr" };
    }

    auto PreviewEnvironment::DaylightPath() -> filesystem::Path {
        filesystem::Path path{ "Resources/Cubemaps/farm_field_puresky_4k.hdr" };
        return path.Exists() ? path : filesystem::Path{ "Mikoto/Resources/Resources/Cubemaps/farm_field_puresky_4k.hdr" };
    }

    auto PreviewEnvironment::Request( const filesystem::Path &path ) -> void {
        auto load{ eastl::make_shared<Load>() };
        threading::TaskService::Get()->ExecuteWorkerTask( [load, path]() {
            asset::AssetHandle<asset::Texture> texture{};
            eastl::string error{};

            try {
                texture = asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D );

                if ( !texture.IsReady() ) {
                    MKT_THROW_RUNTIME_ERROR( "Preview environment could not be loaded." );
                }
            } catch ( const std::exception &exception ) {
                error = exception.what();
            }

            std::lock_guard lock{ load->mMutex };
            load->mTexture = eastl::move( texture );
            load->mError = eastl::move( error );
            load->mComplete = true;
        } );
        mLoad = eastl::move( load );
        mConsumed = false;
        mError.clear();
    }

    auto PreviewEnvironment::Poll( asset::AssetHandle<asset::Texture> &texture ) -> bool {
        if ( !mLoad || mConsumed ) {
            return false;
        }

        std::lock_guard lock{ mLoad->mMutex };

        if ( !mLoad->mComplete ) {
            return false;
        }

        mConsumed = true;
        mError = mLoad->mError;

        if ( mLoad->mTexture ) {
            texture = mLoad->mTexture;
            return true;
        }

        return false;
    }

    auto PreviewEnvironment::Poll( renderer::rhi::TextureHandle &texture ) -> bool {
        asset::AssetHandle<asset::Texture> handle{};

        if ( !Poll( handle ) ) {
            return false;
        }

        const auto resource{ handle.GetIfReady() };

        if ( !resource ) {
            return false;
        }

        texture = resource->GetResource();
        return true;
    }

    auto PreviewEnvironment::IsRequested() const -> bool {
        return mLoad != nullptr;
    }

    auto PreviewEnvironment::GetError() const -> const eastl::string & {
        return mError;
    }
}// namespace mikoto::editor
