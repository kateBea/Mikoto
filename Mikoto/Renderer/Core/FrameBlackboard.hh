//
// Created by kate on 12/19/25.
//

#ifndef MIKOTO_FRAMEBLACKBOARD_HH
#define MIKOTO_FRAMEBLACKBOARD_HH

#include "Renderer/Core/Buffer.hh"
#include "Pipeline.hh"

namespace Mikoto {
    class FrameBlackboard {
    public:



    private:
        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesByNames{};
        ankerl::unordered_dense::map<std::string, PipelineHandle> m_PipelinesByNames{};
        ankerl::unordered_dense::map<std::string, BufferHandle> m_BuffersByNames{};
    };

}

#endif//MIKOTO_FRAMEBLACKBOARD_HH
