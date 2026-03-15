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

#ifndef MIKOTOROOT_ASSET_LOAD_CACHE_HH
#define MIKOTOROOT_ASSET_LOAD_CACHE_HH

#include <Common/Service.hh>

namespace Mikoto {

    class AssetLoadCache : public IService {
    public:

        auto Init() -> void override;
        auto Shutdown() -> void override;

        // Loads async multiple assets
        // If an asset is already enqueued it does not try to request another load

    private:
        // Load model
        // Load texture
        // Load font
        // Load audio

    };

}// namespace Mikoto

#endif//MIKOTOROOT_ASSET_LOAD_CACHE_HH
