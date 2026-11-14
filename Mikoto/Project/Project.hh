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

#ifndef MIKOTO_PROJECT_PROJECT_HH
#define MIKOTO_PROJECT_PROJECT_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Serializer.hh>
#include <Core/ReferenceCounted.hh>

#include <Filesystem/Path.hh>

#include <Assets/Utility.hh>

#include <Renderer/Core/SceneRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/SceneManager.hh>

namespace mikoto::project {

    struct ProjectCreateDescription {
        eastl::string mName{};

        filesystem::Path mAssetsRootDirectory{};
        filesystem::Path mAssetsRegistryDirectory{};// Cache loaded assets

        core::Ref<scene::Scene> mCurrentScene{};
    };

    class Project final : public core::ISerializable, public core::RevisionTracked {
    public:
        explicit Project( eastl::string_view name );
        explicit Project( const ProjectCreateDescription& desc );

        auto Serialize( const filesystem::Path& filename ) const -> void override;
        auto Deserialize( const filesystem::Path& filename ) -> void override;

        auto Serialize( filesystem::FileHandle file ) const -> void override;
        auto Deserialize( filesystem::FileHandle file ) -> void override;

        /** @brief Selects BC6H/BC7 imports or original decoded pixels for this project.
         * @details Persisted as CompressTextures in the project YAML. Applied before loading assets.
         */
        auto SetCompressTextures( bool enabled ) -> void;

        auto SetName( eastl::string_view name ) -> void;

        auto SetAutoSaveInterval( core::f32 milliseconds ) -> void;
        auto SetEnableSaveInterval( bool enable ) -> void;

        auto SetLocation( const filesystem::Path& file ) -> void;
        auto SetScenePath( const filesystem::Path& file ) -> void;

        auto SetSceneRendererSettings( renderer::SceneRenderSettings settings ) -> void;

        /** @brief Associates the camera read during project saves.
         * @param camera Non-owning reference; the camera must outlive saves, or be detached with nullptr.
         * @details First attachment establishes the loaded camera baseline; call RestoreMainCamera first.
         */
        auto SetMainCamera( const scene::Camera* camera ) -> void;

        /** @brief Restores the saved scene-view settings into the supplied camera.
         * @param camera Mutable load target; unsupported camera types are left unchanged.
         */
        auto RestoreMainCamera( scene::Camera& camera ) const -> void;

        /**
         * @brief Reports project settings changed since the last successful load or save.
         * @returns True for an unsaved project, edited persistent properties, or an edited attached camera.
         * @details Renderer edits must be supplied through SetSceneRendererSettings before checking.
         * Saving failures leave the project dirty; checking performs no disk I/O.
         */
        MKT_NODISCARD auto HasUnsavedChanges() const -> bool;

        MKT_NODISCARD auto GetCompressTextures() const -> bool;

        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        /** @brief Creates the game's C# project and Visual Studio solution if absent.
         *
         * Existing project and solution files remain user-owned. Refreshes only
         * the generated engine-reference properties on each call.
         * @param scriptCore Location of the engine's managed API assembly.
         * @return Path to the solution, or an empty path when the API is unavailable.
         */
        MKT_NODISCARD auto EnsureScriptProject( const filesystem::Path& scriptCore ) const -> filesystem::Path;
        MKT_NODISCARD auto GetScriptProjectPath() const -> filesystem::Path;

        MKT_NODISCARD auto GetLocation() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetAssetsDirectory() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetCacheDirectory() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetScenePath() const -> const filesystem::Path&;
        /**
         * @brief Copies sanitized renderer settings and tracks changes to their persistent fields.
         * @details Unchanged values and transient diagnostics do not advance the project revision.
         */
        MKT_NODISCARD auto GetSceneRendererSettings() const -> const renderer::SceneRenderSettings&;

    private:
        eastl::string mName{};
        filesystem::Path mLocation{};
        filesystem::Path mScenePath{};

        filesystem::Path mAssetsRootDirectory{};
        filesystem::Path mAssetsRegistryDirectory{};// Cache loaded assets

        bool mIsAutoSaveEnabled{ false };
        bool mCompressTextures{ false };

        core::f32 mAutoSaveInterval{ 2000 };// In milliseconds

        scene::SceneHandle mCurrentScene{};
        const scene::Camera* mMainCamera{};
        renderer::SceneRenderSettings mSceneRendererSettings{};

        mutable Revision mSerializedRevision{};
        mutable eastl::string mSerializedCameraSettings{};
    };

    using ProjectHandle = core::Ref<Project>;

}

#endif// MIKOTO_PROJECT_PROJECT_HH
