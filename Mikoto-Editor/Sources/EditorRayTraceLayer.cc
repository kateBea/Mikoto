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

#include <Core/LayerStack.hh>

#include <Layers/EditorRayTraceLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;

    EditorRayTraceLayer::EditorRayTraceLayer( platform::Window *window )
        :  ILayer{ "GameLayer" }, mWindow{ window }
    {

    }

    auto EditorRayTraceLayer::OnCreate() -> void {

    }

    auto EditorRayTraceLayer::OnDestroy() -> void {

    }

    auto EditorRayTraceLayer::OnUpdate( float timeStep ) -> void {

    }

    auto EditorRayTraceLayer::OnEvent( core::IEvent &event ) -> void {

    }
}// namespace mikoto::editor