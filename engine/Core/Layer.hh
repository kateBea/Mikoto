/**
 * Layer.hh
 * Created by kate on 5/26/23.
 * */

#ifndef MIKOTO_LAYER_HH
#define MIKOTO_LAYER_HH

// C++ Standard Library
#include <string>
#include <string_view>

// Project Headers
#include <Core/Events/Event.hh>
#include <Utility/Common.hh>

namespace Mikoto {

    /**
     * Describes a modular entity that helps encapsulate and organize
     * functionality of the engine, i.e. subsystems of our engine, e.g.:
     * Editor Layer which encapsulates elements like scene viewport, scene entities hierarchy, etc
     * */
    class Layer {
    public:
        explicit Layer(std::string_view name = "Base Layer")
            :   m_Name{ name } {}

        virtual ~Layer() = default;

        virtual auto OnAttach() -> void = 0;
        virtual auto OnDetach() -> void = 0;
        virtual auto OnUpdate(double ts) -> void = 0;

        virtual auto OnEvent(Event& event) -> void {}
        virtual auto OnImGuiRender() -> void {}

        /**
         * Returns this layer's name (mainly for debugging purposes)
         * @returns this layer's name
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

    private:
        std::string m_Name{};
    };

}

#endif // MIKOTO_LAYER_HH
