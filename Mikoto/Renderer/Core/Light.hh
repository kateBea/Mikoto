//
// Created by zanet on 4/9/2025.
//

#ifndef LIGHT_HH
#define LIGHT_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <glm/glm.hpp>

namespace Mikoto {

    enum class LightType {
        DIRECTIONAL_LIGHT_TYPE,
        POINT_LIGHT_TYPE,
        SPOT_LIGHT_TYPE,
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
        auto GetRadius() const -> float { return m_Radius; }

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

        auto GetDirection() const -> const glm::vec3& { return m_Direction; }

    private:
        glm::vec3 m_Direction{ -1.0f, -1.0f, -1.0f };// default pointing diagonally down
    };

    class SpotLight : public LightObject {
    public:
        explicit SpotLight() = default;
        ~SpotLight() = default;

        template<typename... Args>
        auto SetDirection( Args&&... args ) -> void {
            m_Direction = glm::normalize( glm::vec3( std::forward<Args>( args )... ) );
        }

        auto SetAngle( float angleDeg ) -> void {
            m_Angle = angleDeg;
            UpdateCutoffs();
        }

        auto SetSoftness( float softness ) -> void {
            m_Softness = glm::clamp( softness, 0.0f, 1.0f );
            UpdateCutoffs();
        }

        auto SetRadius( float radius ) -> void { m_Radius = radius; }

        auto GetDirection() const -> const glm::vec3& { return m_Direction; }

        auto GetAngle() const -> float { return m_Angle; }
        auto GetSoftness() const -> float { return m_Softness; }

        auto GetCutOff() const -> float { return m_CutOff; }
        auto GetOuterCutOff() const -> float { return m_OuterCutOff; }

        auto GetRadius() const -> float { return m_Radius; }

    private:
        void UpdateCutoffs() {
            float outer = glm::radians( m_Angle );
            float inner = outer * ( 1.0f - m_Softness );

            m_CutOff = std::cos( inner );
            m_OuterCutOff = std::cos( outer );
        }

    private:
        glm::vec3 m_Direction{ 0.0f, -1.0f, 0.0f };

        float m_Angle{ 30.0f };  // degrees
        float m_Softness{ 0.1f };// 0..1

        float m_CutOff{ 0.0f };
        float m_OuterCutOff{ 0.0f };

        float m_Radius{ 10.0f };
    };
}// namespace Mikoto


#endif//LIGHT_HH
