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

#include <EASTL/sort.h>
#include <EASTL/vector.h>

#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Renderer/Interface/Canvas.hh>
#include <Renderer/Interface/CanvasInput.hh>

#include <Scene/Scene.hh>

namespace mikoto::renderer {

    auto CancelCanvasInput( scene::Scene& scene ) -> void {
        for ( auto [entity, component]: scene.GetRegistry().view<scene::CanvasComponent>().each() ) {
            component.GetCanvas().CancelPointer();
            component.GetCanvas().ClearFocus();
        }
    }

    auto UpdateCanvasInput( scene::Scene& scene, const scene::Camera& camera, core::float2 uv,
        bool down, bool pressed, bool released, bool allowNewPress ) -> bool {
        struct Item {
            Canvas* mCanvas;
            const scene::TransformComponent* mTransform;
            core::u64 mId;
        };
        eastl::vector<Item> items;

        for ( auto [entity, canvas, transform, tag]: scene.GetRegistry().view<scene::CanvasComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
            if ( tag.IsActive() ) {
                items.push_back( { &canvas.GetCanvas(), &transform, tag.GetGuid() } );
            }
            else {
                canvas.GetCanvas().CancelPointer();
                canvas.GetCanvas().ClearFocus();
            }
        }

        // Preserve an active drag first, then hit-test the frontmost visible canvases.
        eastl::sort( items.begin(), items.end(), []( const Item& a, const Item& b ) {
            if ( a.mCanvas->HasPointerCapture() != b.mCanvas->HasPointerCapture() ) {
                return a.mCanvas->HasPointerCapture();
            }

            if ( a.mCanvas->IsOverlay() != b.mCanvas->IsOverlay() ) {
                return a.mCanvas->IsOverlay();
            }

            if ( a.mCanvas->GetSortOrder() != b.mCanvas->GetSortOrder() ) {
                return a.mCanvas->GetSortOrder() > b.mCanvas->GetSortOrder();
            }

            return a.mId > b.mId;
        } );

        bool consumed{};

        for ( const auto& item: items ) {
            auto& canvas = *item.mCanvas;
            // Pointer capture survives leaving the game image. Hover must not
            // leak through an editor window covering an otherwise visible UI.
            if ( consumed || ( !allowNewPress && !canvas.HasPointerCapture() ) ) {
                canvas.CancelPointer();

                if ( pressed ) {
                    canvas.ClearFocus();
                }

                continue;
            }

            const auto [width, height]{ camera.GetViewPort() };
            const auto rootSize{ canvas.GetLayoutSize( { width, height } ) };
            core::float2 position = uv * rootSize;

            if ( !canvas.IsOverlay() ) {
                // Intersect the game-image ray with the canvas's local XY plane.
                const auto& view = canvas.GetCamera() ? *canvas.GetCamera() : camera;
                const auto model = item.mTransform->GetWorldTransform() * glm::scale( core::float4x4{ 1 }, core::float3{ canvas.GetWorldScale(), -canvas.GetWorldScale(), 1 } );
                const auto inverse = glm::inverse( view.GetViewProjection() * model );
                core::float4 nearPoint = inverse * core::float4{ uv.x * 2 - 1, 1 - uv.y * 2, 0, 1 };
                core::float4 farPoint = inverse * core::float4{ uv.x * 2 - 1, 1 - uv.y * 2, 1, 1 };
                nearPoint /= nearPoint.w;
                farPoint /= farPoint.w;
                const float dz = farPoint.z - nearPoint.z;
                const float t = dz != 0 ? -nearPoint.z / dz : -1;

                if ( t < 0 || t > 1 ) {
                    canvas.CancelPointer();
                    continue;
                }

                position = core::float2{ glm::mix( nearPoint, farPoint, t ) };
            }

            consumed = canvas.HandlePointer( position, down, pressed && allowNewPress, released, rootSize );
        }

        return consumed;
    }

    auto NavigateCanvasInput( scene::Scene& scene, CanvasNavigation action ) -> bool {
        struct Candidate {
            Canvas* mCanvas;
            core::u64 mId;
        };
        eastl::vector<Candidate> candidates;

        for ( auto [entity, component, tag]: scene.GetRegistry().view<scene::CanvasComponent, scene::TagComponent>().each() ) {
            auto& canvas{ component.GetCanvas() };

            if ( tag.IsActive() ) {
                candidates.push_back( { &canvas, tag.GetGuid() } );
            }
            else {
                canvas.ClearFocus();
            }
        }

        eastl::sort( candidates.begin(), candidates.end(), []( const Candidate& a, const Candidate& b ) {
            if ( a.mCanvas->HasFocus() != b.mCanvas->HasFocus() ) {
                return a.mCanvas->HasFocus();
            }

            if ( a.mCanvas->IsOverlay() != b.mCanvas->IsOverlay() ) {
                return a.mCanvas->IsOverlay();
            }

            if ( a.mCanvas->GetSortOrder() != b.mCanvas->GetSortOrder() ) {
                return a.mCanvas->GetSortOrder() > b.mCanvas->GetSortOrder();
            }

            return a.mId > b.mId;
        } );

        for ( const auto& candidate: candidates ) {
            if ( candidate.mCanvas->HandleNavigation( action ) ) {
                for ( const auto& other: candidates ) {
                    if ( other.mCanvas != candidate.mCanvas ) {
                        other.mCanvas->ClearFocus();
                    }
                }

                return true;
            }
        }

        return false;
    }

}// namespace mikoto::renderer
