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

#ifndef MIKOTO_THUMBNAIL_RENDERER_HH
#define MIKOTO_THUMBNAIL_RENDERER_HH

#include <Renderer/Core/Renderer.hh>

namespace Mikoto {

    // Not sure how I will tackle this, my first idea is to submit the scenes with a single model
    // to this renderer, render the objects with low resolution and then save the result to a file in disk
    // and just use that saved image as the thumbnail
    class ThumbnailRenderer final : public Renderer {
    public:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Render( Scene * ) -> void override;

    private:
    };

}// namespace Mikoto

#endif//MIKOTO_THUMBNAIL_RENDERER_HH
