//
// Created by zanet on 4/9/2025.
//

#ifndef LIGHT_HH
#define LIGHT_HH

#include <glm/glm.hpp>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class LightType {
        DIRECTIONAL_LIGHT_TYPE,
        POINT_LIGHT_TYPE,
        SPOT_LIGHT_TYPE,
    };


    class LightObject {
    public:

        template<typename... Args>
        auto SetColor(Args&&... args) -> void  {
            m_Color = glm::vec3(std::forward<Args>(args)...);
        }

    protected:
        Vec3F m_Color{};
    };

/**
 * @brief Represents a light source in the scene.
 *
 * This class encapsulates the properties and behavior of a light source,
 * including its position, color, intensity, and other relevant attributes.
 */
    class Light {
        public:
            enum class Type {
                DIRECTIONAL,
                POINT,
                SPOT,
            };

            Light() = default;

            auto SetType(Type type) -> void { m_Type = type; }
            auto GetType() const -> Type { return m_Type; }

            auto SetColor(const Vec3F& color) -> void { m_Color = color; }
            auto GetColor() const -> Vec3F { return m_Color; }

            auto SetIntensity(float intensity) -> void { m_Intensity = intensity; }
            auto GetIntensity() const -> float { return m_Intensity; }

            private:
            Type m_Type{ Type::DIRECTIONAL };
            Vec3F m_Color{ 1.0f, 1.0f, 1.0f };
            float m_Intensity{ 1.0f };
    };

    class PointLight : public LightObject {
    public:
        explicit PointLight() = default;

        ~PointLight() = default;

        template<typename... Args>
        auto SetPosition(Args&&... args) -> void {
            m_Position = glm::vec3(std::forward<Args>(args)...);
        }

        auto SetRadius(float radius) -> void {
            m_Radius = radius;
        }

        auto SetIntensity(float intensity) -> void {
            m_Intensity = intensity;
        }

        auto GetPosition() const -> const glm::vec3& {
            return m_Position;
        }

        auto GetRadius() const -> float {
            return m_Radius;
        }

        auto GetIntensity() const -> float {
            return m_Intensity;
        }

    private:

        glm::vec3 m_Position{};
        float m_Intensity{ 0.0f };
        float m_Radius{ 0.0f };
    };

    class DirectionalLight : public LightObject {
    public:
        explicit DirectionalLight() = default;

        ~DirectionalLight() = default;

        template<typename... Args>
        auto SetDirection(Args&&... args) -> void {
            m_Direction = glm::vec3(std::forward<Args>(args)...);
        }

        auto SetIntensity(float intensity) -> void {
            m_Intensity = intensity;
        }

        auto SetPosition(float x, float y, float z) -> void {
            m_Position = glm::vec3(x, y, z);
        }

        auto GetDirection() const -> const glm::vec3& {
            return m_Direction;
        }

        auto GetPosition() const -> const glm::vec3& {
            return m_Position;
        }


    private:
        glm::vec3 m_Direction{ 1.0f, 1.0f, 1.0f };
        glm::vec3 m_Position{ 1.0f, 1.0f, 1.0f };
        float m_Intensity{ 0.0f };
    };

    class SpotLight : public LightObject {
    public:
        explicit SpotLight() = default;

        ~SpotLight() = default;

        template<typename... Args>
        auto SetDirection(Args&&... args) -> void {
            m_Direction = glm::vec4(std::forward<Args>(args)...);
        }

        auto SetCutOff(float cutOff) -> void;

        auto SetOuterCutOff(float outerCutOff) -> void;

        auto SetRadius(float radius) -> void;

        template<typename... Args>
        auto SetPosition(Args&&... args) -> void {
            m_Position = glm::vec3(std::forward<Args>(args)...);
        }

        auto SetIntensity(float intensity) -> void;

    private:
        glm::vec3 m_Position{};
        glm::vec3 m_Direction{ 0.0f, -1.0f, 0.0f }; // facing down by default
        float m_CutOff{ 0.2f };
        float m_OuterCutOff{ 0.4f };
        float m_Intensity{ 0.0f };
        float m_Radius{ 0.0f };

    };
}



#endif //LIGHT_HH
