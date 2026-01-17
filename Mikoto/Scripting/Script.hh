//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_SCRIPT_HH
#define MIKOTO_SCRIPT_HH

#include <sol/sol.hpp>

#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>

#include <Scene/Entity.hh>

namespace Mikoto {

    class Script final : public IResource {
    public:
        explicit Script(const File* file, sol::state& state, Entity* entity);

        auto Update(float dt ) -> void;

        auto SetEnable(bool value) -> void;

        MKT_NODISCARD auto IsEnabled() const -> bool;
        MKT_NODISCARD auto GetFile() const -> const File*;

        ~Script() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto OnCreate() -> void;
        auto OnUpdate(float dt) -> void;

    private:

        Entity* m_Entity{};

        sol::state* m_State{};
        const File* m_File{};

        bool m_Enabled{ false };

        sol::table m_Object{};
        sol::function m_OnCreate{};
        sol::function m_OnUpdate{};
    };

    using ScriptHandle = Ref<Script>;

}

#endif//MIKOTO_SCRIPT_HH
