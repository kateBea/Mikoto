//
// Created by zanet on 1/10/2026.
//

#ifndef MIKOTO_SHADER_RENDER_PARAMS_HH
#define MIKOTO_SHADER_RENDER_PARAMS_HH

#include <string>
#include <string_view>
#include <vector>
#include <array>

#include <glm/glm.hpp>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    static constexpr UInt32 MAX_LIGHTS{ 5096 };
    static constexpr UInt32 MAX_CUBE_MAP_FACES{ 6 };
    static constexpr UInt32 MAX_RENDERABLE_ENTITIES{ 4096 * 10 };

    struct ShaderMaterialParams {
        Mat4F Transform{};

        Vec4F Albedo{};
        Vec4F Factors{};

        Int32 AlbedoIndex{};
        Int32 NormalIndex{};
        Int32 MetallicIndex{};
        Int32 RoughnessIndex{};
        Int32 AoIndex{};
    };

    struct ShaderCameraParams {
        glm::mat4 View{};
        glm::mat4 Projection{};
        Vec4F CameraPosition{};
    };

    struct alignas(sizeof(Vec4F)) ShaderLightTypeParams {
        Vec4F Position{};
        Vec4F Direction{};

        Vec3F Diffuse{};

        float CutOff{};
        float OuterCutOff{};
        float Intensity{};
        float Radius{};

        Int32 ActiveLightType{};
    };

    enum class ShaderColorDisplayMode {
        NORMAL = 1,
        COLOR = 2,
        METAL = 3,
        AO = 4,
        ROUGH = 5,
    };

    enum class ShaderActiveLightType {
        LIGHT_TYPE_INACTIVE = -1,
        LIGHT_TYPE_POINT = 1,
        LIGHT_TYPE_SPOT = 2,
        LIGHT_TYPE_DIRECTIONAL = 3,
    };

    struct ShaderLightListParams {
        std::vector<ShaderLightTypeParams> Lights{
            MAX_LIGHTS,
            ShaderLightTypeParams()
        };

        Int32 ActiveLightsCount{};
        Int32 DisplayMode{};
    };

}

#endif //MIKOTO_SHADER_RENDER_PARAMS_HH