//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAMEGRAPH_HH
#define MIKOTO_FRAMEGRAPH_HH

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Data/Registry.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/Pipeline.hh>

namespace Mikoto {

    class FrameGraph final {
    public:
        template<typename PassType, typename... Args>
        auto RegisterPass(Args&&... args) -> void {
            m_Passes.Register<PassType>( std::forward<Args>(args)...  );
        }

        template<typename PassType>
        auto UnRegisterPass() -> void {
            m_Passes.Unregister<PassType>();
        }


        auto Compile(GraphicsContext& backend) -> void;

        auto Execute(GraphicsContext& backend) -> void;

    private:
        Registry<FramePass> m_Passes{};

        bool m_Compiled{ false };
    };
}

#endif//MIKOTO_FRAMEGRAPH_HH
