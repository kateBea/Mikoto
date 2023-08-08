/**
 * Layer.hh
 * Created by kate on 5/26/23.
 * */

#ifndef KATE_ENGINE_LAYER_HH
#define KATE_ENGINE_LAYER_HH

// C++ Standard Library
#include <string>
#include <string_view>

// Project Headers
#include <Core/Events/Event.hh>
#include <Utility/Common.hh>

namespace kaTe {

    /**
     * Describes a modular entity that helps encapsulate and organize
     * functionality of the engine, i.e. subsystems of our engine, e.g.:
     * Editor Layer which encapsulates elements like scene viewport, scene entities hierarchy, etc
     *
     * This class serves simply as a general interface for those Layers
     * */
    class Layer {
    public:
        explicit Layer(std::string_view name = "Base Layer")
            :   m_Name{ name } {}

        virtual ~Layer() = default;

        virtual auto OnAttach() -> void = 0;
        virtual auto OnDetach() -> void = 0;
        virtual auto OnUpdate(double ts) -> void = 0;

        // These functions are defined here because a layer may not need them,
        // therefore, it should not provide a definition for them
        virtual auto OnEvent([[maybe_unused]] Event& event) -> void {}
        virtual auto OnImGuiRender()                        -> void {}

        /**
         * For debugging purposes
         * */
        KT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

    private:
        std::string m_Name{};
    };

}

#endif // KATE_ENGINE_LAYER_HH
