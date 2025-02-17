/**
 * Shader.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_BASE_SHADER_HH
#define MIKOTO_BASE_SHADER_HH

// C++ Standard Library

// Remember to pass in the stage name not a symbol name e.g., a variable
#define SHADER_STAGE_STR(STAGE_NAME) #STAGE_NAME

// Project Headers
#include <Models/Enums.hh>
#include <Common/Common.hh>

namespace Mikoto {

    class Shader {
    public:
        explicit Shader( const ShaderStage stage)
            :   m_Stage{ stage } {

        }

        MKT_NODISCARD auto GetStage() const -> ShaderStage { return m_Stage; }

        virtual ~Shader() = default;

    protected:
        ShaderStage m_Stage{};

    };
}

#endif // MIKOTO_BASE_SHADER_HH
