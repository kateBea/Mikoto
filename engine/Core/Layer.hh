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
#include <Core/Event.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    /**
     * Describes a modular entity that helps encapsulate and organize
     * functionality of the engine, i.e. subsystems of our engine, e.g.:
     * Editor Layer which encapsulates elements like scene viewport, scene
     * entities hierarchy, etc.
     * */
    class Layer {
    public:
        explicit Layer(std::string_view name = "Base Layer")
            :   m_Name{ name } {}

        virtual auto OnAttach() -> void = 0;
        virtual auto OnDetach() -> void = 0;
        virtual auto OnUpdate(double ts) -> void = 0;

        // change Event& to Event*
        virtual auto OnEvent(Event& event) -> void { (void)event; }
        virtual auto PushImGuiDrawItems() -> void {}

        /**
         * Returns this layer's name (mainly for debugging purposes)
         * @returns this layer's name
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        /**
         * Compare two Layers and returns true if they are the same or not. Two layers
         * are the same if they have the same name.
         * @param other layer to compare to
         * @returns true if <code>other</code> and the implicit parameter are the same layer, false otherwise
         * */
        auto operator==(const Layer& other) const noexcept -> bool { return m_Name == other.GetName(); }

        virtual ~Layer() = default;

    private:
        std::string m_Name{};
    };
}

#endif // MIKOTO_LAYER_HH
