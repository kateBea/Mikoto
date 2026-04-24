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

#ifndef MIKOTO_SCENE_SERIALIZER_HH
#define MIKOTO_SCENE_SERIALIZER_HH

#include <EASTL/unique_ptr.h>

#include <Core/Serializer.hh>
#include <Scene/Scene.hh>

namespace mikoto::scene {

    class SceneSerializer final : public ISerializer<Scene> {
    public:
        auto Serialize( const Scene& scene, const Path& saveFilePath ) -> void override;
        auto Deserialize( const Path& saveFilePath ) -> eastl::unique_ptr<Scene> override;
    };
}
#endif // MIKOTO_SCENE_SERIALIZER_HH
