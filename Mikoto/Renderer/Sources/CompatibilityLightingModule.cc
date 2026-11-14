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

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/CompatibilityRenderer/Passes/CompatibilityLightingModule.hh>

namespace mikoto::renderer {
    auto CompatibilityLightingModule::Gather( const scene::Scene* scene, Lights& lights ) const -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( !scene ) {
            return 0;
        }

        core::usize count{};
        for ( const auto& [entity, tag, transform, component]:
            scene->GetRegistry().view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>().each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }

            const auto& world{ transform.GetWorldTransform() };
            CompatibilityLight light{};
            light.mPositionType = core::float4{ core::float3{ world[3] }, 0.0f };
            switch ( component.GetActiveType() ) {
                case LightType::eDirectional: {
                    const auto& source{ component.Get<DirectionalLight>() };
                    light.mDirectionRadius = core::float4{ source.GetWorldDirection( world ), 0.0f };
                    light.mRadiance = core::float4{ source.GetColor() * source.GetIntensity(), 0.0f };
                    break;
                }
                case LightType::ePoint: {
                    const auto& source{ component.Get<PointLight>() };
                    light.mPositionType.w = 1.0f;
                    light.mDirectionRadius.w = source.GetRadius();
                    light.mRadiance = core::float4{ source.GetColor() * source.GetIntensity(), 0.0f };
                    break;
                }
                case LightType::eSpot: {
                    const auto& source{ component.Get<SpotLight>() };
                    light.mPositionType.w = 2.0f;
                    light.mDirectionRadius = core::float4{ source.GetWorldDirection( world ), source.GetRadius() };
                    light.mRadiance = core::float4{ source.GetColor() * source.GetIntensity(), 0.0f };
                    light.mSpot = core::float4{ source.GetCutOff(), source.GetOuterCutOff(), 0.0f, 0.0f };
                    break;
                }
                default:
                    continue;
            }

            lights[count++] = light;
            if ( count == lights.size() ) {
                break;
            }
        }

        return count;
    }
}
