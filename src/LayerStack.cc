/**
 * LayerStack.cc
 * Created by kate on 5/26/23.
 * */

// C++ Standard Library
#include <algorithm>

// Project headers
#include <Core/Logger.hh>
#include <Core/LayerStack.hh>

namespace Mikoto {
    auto LayerStack::AddLayer(const std::shared_ptr<Layer>& layer) -> void {
        m_Layers.emplace(m_Layers.begin() + m_LayerIndex, layer);
        layer->OnAttach();
        ++m_LayerIndex;
    }

    auto LayerStack::AddOverlay(const std::shared_ptr<Layer>& overlay) -> void {
        m_Layers.emplace_back(overlay);
    }

    auto LayerStack::PopLayer(const std::shared_ptr<Layer>& layer) -> void {
        auto targetIt{ std::find(m_Layers.begin(), m_Layers.end(), layer) };
        if (targetIt != m_Layers.end()) {
            m_Layers.erase(targetIt);
            --m_LayerIndex;
        }

    }

    auto LayerStack::PopOverlay(const std::shared_ptr<Layer>& overlay) -> void {
        auto targetIt{ std::find(m_Layers.begin(), m_Layers.end(), overlay) };
        if (targetIt != m_Layers.end())
            m_Layers.erase(targetIt);

    }

    auto LayerStack::Init() -> void {
        MKT_CORE_LOGGER_INFO("Layer Stack initialization");
    }

    auto LayerStack::ShutDown() -> void {

    }

}