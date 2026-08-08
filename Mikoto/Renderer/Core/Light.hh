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

#ifndef MIKOTO_LIGHT_HH
#define MIKOTO_LIGHT_HH

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    enum class LightType {
        eDirectional,
        ePoint,
        eSpot,
        eAreaLight,
        eSkyLight
    };

    // Base class for shared light data (color)
    class LightObject {
    public:
        template<typename... Args>
        auto SetColor( Args&&... args ) -> void {
            m_Color = glm::vec3( std::forward<Args>( args )... );
        }

        auto SetColor( const glm::vec3& color ) -> void { m_Color = color; }
        auto GetColor() const -> const glm::vec3& { return m_Color; }

        auto SetIntensity( float intensity ) -> void { m_Intensity = intensity; }
        auto GetIntensity() const -> float { return m_Intensity; }

    protected:
        glm::vec3 m_Color{ 1.0f, 1.0f, 1.0f };
        float m_Intensity{ 1.0f };
    };

    class PointLight : public LightObject {
    public:
        explicit PointLight() = default;
        ~PointLight() = default;

        auto SetRadius( float radius ) -> void { m_Radius = radius; }
        MKT_NODISCARD auto GetRadius() const -> float { return m_Radius; }

    private:
        float m_Radius{ 1.0f };// falloff radius
    };

    class DirectionalLight : public LightObject {
    public:
        explicit DirectionalLight() = default;
        ~DirectionalLight() = default;

        template<typename... Args>
        auto SetDirection( Args&&... args ) -> void {
            m_Direction = glm::vec3( std::forward<Args>( args )... );
        }

        MKT_NODISCARD auto GetDirection() const -> const glm::vec3& { return m_Direction; }

    private:
        glm::vec3 m_Direction{ -1.0f, -1.0f, -1.0f };// default pointing diagonally down
    };

    class SpotLight : public LightObject {
    public:
        explicit SpotLight() { UpdateCutoffs(); }

        template<typename... Args>
        auto SetDirection( Args&&... args ) -> void {
            m_Direction = glm::normalize( glm::vec3( std::forward<Args>( args )... ) );
        }

        auto SetAngle( float angleDeg ) -> void {
            m_Angle = angleDeg;
            UpdateCutoffs();
        }

        auto SetSoftness( float softness ) -> void {
            m_Softness = glm::clamp( softness, 0.0f, GetMaxSoftness() );
            UpdateCutoffs();
        }

        auto SetRadius( float radius ) -> void { m_Radius = radius; }

        MKT_NODISCARD static auto GetMaxSoftness() -> float { return 30.0f; }
        MKT_NODISCARD static auto GetMaxAngle() -> float { return 180.0f; } // In degrees

        MKT_NODISCARD auto GetDirection() const -> const glm::vec3& { return m_Direction; }

        MKT_NODISCARD auto GetAngle() const -> float { return m_Angle; }
        MKT_NODISCARD auto GetSoftness() const -> float { return m_Softness; }

        MKT_NODISCARD auto GetCutOff() const -> float { return m_CutOff; }
        MKT_NODISCARD auto GetOuterCutOff() const -> float { return m_OuterCutOff; }

        MKT_NODISCARD auto GetRadius() const -> float { return m_Radius; }

    private:
        auto UpdateCutoffs() -> void {
            const float outer{ glm::radians( m_Angle ) };
            const float inner{ outer * ( GetMaxSoftness() - m_Softness ) };

            m_CutOff = glm::cos( inner );
            m_OuterCutOff = glm::cos( outer );
        }

    private:
        float3 m_Direction{ 0.0f, -1.0f, 0.0f }; // Pointing downwards by default

        float m_Angle{ 30.0f };  // degrees
        float m_Softness{ 1.0f };

        float m_CutOff{ 0.0f };
        float m_OuterCutOff{ 0.0f };

        float m_Radius{ 10.0f };
    };
}// namespace Mikoto


#endif//MIKOTO_LIGHT_HH
