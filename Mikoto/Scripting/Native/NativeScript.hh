//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_SCRIPTING_NATIVE_SCRIPT_HH
#define MIKOTO_SCRIPTING_NATIVE_SCRIPT_HH

#include <EASTL/shared_ptr.h>

#include <Scripting/Script.hh>
#include <Scripting/Native/NativeScriptApi.hh>

struct SDL_SharedObject;

namespace mikoto::scripting {

    struct NativeScriptModule {
        SDL_SharedObject* mLibrary{};
        const native::ScriptModule* mApi{};
        filesystem::Path mLoadedPath{};
        ScriptClassMetadata mMetadata{};

        auto ReadMetadata() -> bool;
        ~NativeScriptModule();
    };

    /** @brief Owns an entity's instance and retains its module until that instance is destroyed. */
    class NativeScript final : public IScript {
    public:
        explicit NativeScript( filesystem::FileHandle file, scene::Entity *entity );

        ~NativeScript() override;

        auto Start( scene::Scene &scene ) -> void override;

        auto Stop() -> void override;
        auto Update( float timeStep ) -> void override;
        auto OnPhysicsEvent( const physics::ContactEvent &event, scene::Entity &other ) -> void override;
        auto ReloadScript() -> void override;

        MKT_NODISCARD auto GetFile() const -> filesystem::FileHandle override;
        MKT_NODISCARD auto GetEntity() const -> scene::Entity * override;
        MKT_NODISCARD auto GetLanguage() const -> ScriptLanguage override;
        MKT_NODISCARD auto GetMetaData() const -> const ScriptClassMetadata& override;

    private:
        auto ApplyProperties() -> bool;
        auto StartInstance() -> void;
        auto AdoptModule( eastl::shared_ptr<NativeScriptModule> module ) -> void;

        scene::Entity *mEntity{};

        // File ownership is independent of the borrowed scene entity.
        filesystem::FileHandle mFile{};
        eastl::shared_ptr<NativeScriptModule> mModule{};
        mutable eastl::shared_ptr<NativeScriptModule> mMetadataModule{};
        void* mInstance{};
        eastl::string mAppliedProperties{};
        bool mPlaying{};
        bool mRunning{};
        bool mUpdateFailed{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_NATIVE_SCRIPT_HH
