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

#ifndef MIKOTO_SCRIPTING_SCRIPT_HH
#define MIKOTO_SCRIPTING_SCRIPT_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/optional.h>

#include <ankerl/unordered_dense.h>

#include <Core/ReferenceCounted.hh>

#include <Filesystem/File.hh>

namespace mikoto::physics {
    struct ContactEvent;
}

namespace mikoto::scene {
    class Entity;
    class Scene;
}// namespace mikoto::scene

namespace mikoto::scripting {

    /** @brief Language implemented by a script resource. */
    enum class ScriptLanguage : core::u32 {
        eUnknown,
        eNative,
        eLua,
        eCSharp
    };

    enum class ScriptValueType : core::u32 {
        eUnknown,

        eBool,

        eInt32,
        eUInt32,

        eFloat32,
        eFloat64,

        eString,

        eVec2,
        eVec3,
        eVec4,
        eEntity,
        eAsset,
    };

    struct ScriptFieldMetadata {
        core::u32 mFieldID{};

        eastl::string mName{};
        ScriptValueType mType{ ScriptValueType::eUnknown };

        eastl::string mAssetKind{};
        eastl::string mDefaultJson{ "null" };

        bool mReadOnly{ false };
        bool mSerializable{ true };
    };

    class ScriptClassMetadata {
    public:
        core::u32 mClassID{};

        eastl::string mName{};
        eastl::string mNamespace{};

        ScriptLanguage mLanguage{ ScriptLanguage::eUnknown };

        ankerl::unordered_dense::map<core::u32, ScriptFieldMetadata> mFieldLookup{};
    };

    /** @brief Common script contract; language implementations own their execution state.
     *
     * ScriptHandle retains implementations through ReferenceCounted<IScript>.
     * Managed execution remains scene-scoped; native execution is not implemented yet.
     */
    class IScript : public core::ReferenceCounted<IScript> {
    public:
        /** @brief Attaches the script to a playing scene.
         *
         * @param scene Scene providing the execution context.
         */
        virtual auto Start( scene::Scene &scene ) -> void = 0;

        /** @brief Stops execution before the owning entity or scene is destroyed. */
        virtual auto Stop() -> void = 0;

        /** @brief Advances per-script execution where supported.
         *
         * @param timeStep Elapsed frame time in seconds.
         */
        virtual auto Update( float timeStep ) -> void = 0;

        /** Receives body-pair notifications on the engine thread, outside solver callbacks. */
        virtual auto OnPhysicsEvent( const physics::ContactEvent &, scene::Entity & ) -> void {
        }

        /** @brief Reloads source where supported by the implementation. */
        virtual auto ReloadScript() -> void = 0;

        /** @brief Retrieves the retained source or assembly file.
         *
         * @return File associated with this script.
         */
        MKT_NODISCARD virtual auto GetFile() const -> filesystem::FileHandle = 0;

        /** @brief Retrieves the entity associated with this script.
         *
         * @return Borrowed entity pointer; the entity must outlive script execution.
         */
        MKT_NODISCARD virtual auto GetEntity() const -> scene::Entity * = 0;

        /** @brief Identifies the implementation's language.
         *
         * @return Lua, C# or native language identifier.
         */
        MKT_NODISCARD virtual auto GetLanguage() const -> ScriptLanguage = 0;

        /** @brief Return script metadata.
         *
         * @return Information about this script.
         */
        MKT_NODISCARD virtual auto GetMetaData() const -> const ScriptClassMetadata& = 0;
    };

    using ScriptHandle = core::Ref<IScript>;
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_SCRIPT_HH
