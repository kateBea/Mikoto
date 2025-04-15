//
// Created by zanet on 4/9/2025.
//

#ifndef SAMPLER_HH
#define SAMPLER_HH

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/GpuUtility.hh>

namespace Mikoto {



    /**
    * @brief Represents a sampler object used for texture sampling.
    *
    * This class encapsulates the functionality of a sampler, allowing for
    * texture sampling with various filtering and wrapping modes.
    */
    class Sampler : public DeviceObject {
    public:


    protected:
        SamplerFilter m_Filter{ SamplerFilter::FILTER_LINEAR };
        SamplerWrapMode m_Wrap{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE };
    };
}



#endif //SAMPLER_HH
