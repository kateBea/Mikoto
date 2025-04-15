//
// Created by zanet on 4/9/2025.
//

#ifndef IPIPELINE_HH
#define IPIPELINE_HH

#include <string>

#include <Common/Common.hh>
#include <Renderer/GpuUtility.hh>
#include <Renderer/DeviceObject.hh>


namespace Mikoto {
    class IPipeline : public DeviceObject {
    public:
        ~IPipeline() override = default;

        MKT_NODISCARD auto GetPipelineType() const -> PipelineType {
            return m_PipelineType;
        }

        MKT_NODISCARD auto GetPassName() const -> std::string {
            return m_PassName;
        }

    protected:
        std::string m_PassName{};
        PipelineType m_PipelineType{ PipelineType::INVALID_TYPE };
    };
}
#endif //IPIPELINE_HH
