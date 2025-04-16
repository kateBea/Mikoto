//
// Created by zanet on 4/9/2025.
//

#ifndef LIGHT_HH
#define LIGHT_HH

#include <glm/glm.hpp>

namespace Mikoto {

    enum class LightType {
        DIRECTIONAL_LIGHT_TYPE = 0,
        POINT_LIGHT_TYPE = 1,
        SPOT_LIGHT_TYPE = 2,
    };


    struct DirectionalLight {
        // if Direction.w == 1, we use the lights position
        // to compute the rays directions
        glm::vec4 Direction{ 1.0f, 1.0f, 1.0f, 0.0f };
        glm::vec4 Position{ 1.0f, 1.0f, 1.0f, 1.0f };

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    // Point lights for now
    struct PointLight {
        glm::vec4 Position{ 0.0f, 0.0f, 0.0f, 1.0f };

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 0.1f };

        // x = intensity, y = radius
        glm::vec4 AttenuationParams{ 1.0f, 1.0f, 1.0f, 0.1f };

    };

    struct SpotLight {
        glm::vec4 Position{};
        glm::vec4 Direction{ 0.0f, -1.0f, 0.0f, 0.0f }; // facing down by default

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 0.1f };

        // cutoff
        // x=cutOff, y=outerCutOff, z = intensity, w = radius
        glm::vec4 Params{ 0.2f, 0.4f, 0.0f, 0.0f };
    };

    /**
     * @brief Holds light related information.
     * Contains the relevant data specific for the three types of light:
     * directional light, spot light, point light. Used to store light information
     * in the light component.
     * */
    struct LightData {
        // Directional light
        DirectionalLight DireLightData{};

        // Point light
        PointLight PointLightDat{};

        // Spotlight
        SpotLight SpotLightData{};
    };

    class LightObject {
    public:

        template<typename... Args>
        auto SetColor(Args&&... args) -> void  {
            m_Color = glm::vec3(std::forward<Args>(args)...);
        }

    protected:
        glm::vec3 m_Color{};
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

            auto SetColor(const Vec3& color) -> void { m_Color = color; }
            auto GetColor() const -> Vec3 { return m_Color; }

            auto SetIntensity(float intensity) -> void { m_Intensity = intensity; }
            auto GetIntensity() const -> float { return m_Intensity; }

            private:
            Type m_Type{ Type::DIRECTIONAL }; /**< The type of the light source. */
            Vec3 m_Color{ 1.0f, 1.0f, 1.0f }; /**< The color of the light source. */
            floar m_Intensity{ 1.0f }; /**< The intensity of the light source. */
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
