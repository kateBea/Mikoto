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

#include <tuple>
#include <fstream>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>


#include <Scene/SceneCamera.hh>

#include <Project/Project.hh>

namespace mikoto::project {

    using namespace mikoto::filesystem;

    MKT_NODISCARD static auto SerializeCameraSettings( const scene::SceneCamera& camera ) -> YAML::Node {
        YAML::Node node;
        node["NavigationMode"] = camera.GetNavigationMode() == scene::CameraNavigationMode::eOrbit ? "Orbit" : "Fly";
        node["OrbitDistance"] = camera.GetOrbitDistance();

        for ( int axis{}; axis < 3; ++axis ) {
            node["Position"].push_back( camera.GetPosition()[axis] );
            node["Direction"].push_back( camera.GetTargetForward()[axis] );
        }

        node["Position"].SetStyle( YAML::EmitterStyle::Flow );
        node["Direction"].SetStyle( YAML::EmitterStyle::Flow );
        node["Projection"] = camera.IsOrthographic() ? "Orthographic" : "Perspective";
        node["FieldOfView"] = camera.GetFOV();
        node["OrthographicHeight"] = camera.GetOrthographicHeight();
        node["NearPlane"] = camera.GetNearPlane();
        node["FarPlane"] = camera.GetFarPlane();
        node["MovementSpeed"] = camera.GetMovementSpeed();
        node["RotationSpeed"] = camera.GetRotationSpeed();
        node["SmoothTime"] = camera.GetSmoothTime();
        node["AllowPitch"] = camera.WantsXAxisRotation();
        node["AllowYaw"] = camera.WantsYAxisRotation();
        return node;
    }

    MKT_NODISCARD static auto CaptureCameraSettings( const scene::Camera* camera ) -> eastl::string {
        if ( const auto* sceneCamera{ dynamic_cast<const scene::SceneCamera*>( camera ) } ) {
            return YAML::Dump( SerializeCameraSettings( *sceneCamera ) ).c_str();
        }

        return {};
    }

    static auto DeserializeCameraSettings( scene::SceneCamera& camera, const YAML::Node& node ) -> void {
        camera.ResetSettings();
        camera.ResetView();
        // Older projects have no camera mapping. The viewport and input window stay live.
        if ( !node || !node.IsMap() ) {
            return;
        }

        const auto scalar{ [&]( core::cstr key, core::f32 fallback, core::f32 minimum, core::f32 maximum ) {
            const auto value{ node[key].as<core::f32>( fallback ) };
            return glm::clamp( value, minimum, maximum );
        } };
        const auto vector{ [&]( core::cstr key, const core::float3& fallback ) -> core::float3 {
            const auto value{ node[key] };

            if ( !value || !value.IsSequence() || value.size() != 3 ) {
                return fallback;
            }

            core::float3 result{};

            for ( int axis{}; axis < 3; ++axis ) {
                result[axis] = value[axis].as<core::f32>( fallback[axis] );
            }

            return result;
        } };
        camera.SetNavigationMode( node["NavigationMode"].as<std::string>( "Fly" ) == "Orbit" ? scene::CameraNavigationMode::eOrbit : scene::CameraNavigationMode::eFly );
        camera.SetProjectionType( node["Projection"].as<std::string>( "Perspective" ) == "Orthographic" ? scene::ProjectionType::eOrthographic : scene::ProjectionType::ePerspective );
        camera.SetFieldOfView( scalar( "FieldOfView", camera.GetFOV(), 1.0f, 170.0f ) );
        camera.SetOrthographicHeight( scalar( "OrthographicHeight", camera.GetOrthographicHeight(), 0.01f, 1000000.0f ) );
        const auto nearPlane{ scalar( "NearPlane", camera.GetNearPlane(), 0.001f, 100000.0f ) };
        camera.SetClipPlanes( nearPlane, scalar( "FarPlane", camera.GetFarPlane(), nearPlane + 0.01f, 1000000.0f ) );
        camera.SetMovementSpeed( scalar( "MovementSpeed", camera.GetMovementSpeed(), 0.01f, 5000.0f ) );
        camera.SetRotationSpeed( scalar( "RotationSpeed", camera.GetRotationSpeed(), 0.1f, 500.0f ) );
        camera.SetSmoothTime( scalar( "SmoothTime", camera.GetSmoothTime(), 0.0f, 2.0f ) );
        camera.WantRotation( node["AllowPitch"].as<bool>( camera.WantsXAxisRotation() ), node["AllowYaw"].as<bool>( camera.WantsYAxisRotation() ) );
        const auto position{ glm::clamp( vector( "Position", camera.GetPosition() ), core::float3{ -1000000.0f }, core::float3{ 1000000.0f } ) };
        auto direction{ vector( "Direction", camera.GetTargetForward() ) };
        const float lengthSquared{ glm::dot( direction, direction ) };

        if ( lengthSquared < 0.000001f ) {
            direction = camera.GetTargetForward();
        }

        camera.SetViewPose( position, direction );
        camera.SetOrbitDistance( scalar( "OrbitDistance", camera.GetOrbitDistance(), 0.01f, 1000000.0f ) );
    }

    static auto WriteRendererSettings( YAML::Emitter& out, renderer::SceneRenderSettings settings ) -> void {
        settings.Sanitize();
        out << YAML::Key << "Renderer" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "Resolution" << YAML::Value << static_cast<unsigned>( settings.mResolution );
        out << YAML::Key << "Samples" << YAML::Value << ( 1u << static_cast<unsigned>( settings.mMultisampling ) );
        out << YAML::Key << "ShadowQuality" << YAML::Value << static_cast<unsigned>( settings.mShadowQuality );
        out << YAML::Key << "ShadowDistance" << YAML::Value << settings.mShadowDistance;
        out << YAML::Key << "CascadedShadows" << YAML::Value << settings.mCascadedShadowsEnabled;
        out << YAML::Key << "SoftShadows" << YAML::Value << settings.mSoftShadowsEnabled;
        // Persist tracing modes beside the sampling budgets they control.
        out << YAML::Key << "PathTracing" << YAML::Value << settings.mRayTracing.mPathTracingEnabled;
        out << YAML::Key << "RayTracedShadows" << YAML::Value << settings.mRayTracing.mShadowsEnabled;
        out << YAML::Key << "RayTracedReflections" << YAML::Value << settings.mRayTracing.mReflectionsEnabled;
        out << YAML::Key << "RayTracedGlobalIllumination" << YAML::Value << settings.mRayTracing.mGlobalIlluminationEnabled;
        out << YAML::Key << "RayTracedAmbientOcclusion" << YAML::Value << settings.mRayTracing.mAmbientOcclusionEnabled;
        out << YAML::Key << "AoCosineSampling" << YAML::Value << settings.mRayTracing.mUseCosineWeightedAoSampling;
        out << YAML::Key << "AoSamplesPerFrame" << YAML::Value << settings.mRayTracing.mAoSamplesPerFrame;
        out << YAML::Key << "AoMaxDistance" << YAML::Value << settings.mRayTracing.mAoMaxDistance;
        out << YAML::Key << "RayGiSamplesPerFrame" << YAML::Value << settings.mRayTracing.mRayGiSamplesPerFrame;
        out << YAML::Key << "RayGiMaxBounces" << YAML::Value << settings.mRayTracing.mRayGiMaxBounces;
        out << YAML::Key << "PathSamplesPerFrame" << YAML::Value << settings.mRayTracing.mPathSamplesPerFrame;
        out << YAML::Key << "PathMaxBounces" << YAML::Value << settings.mRayTracing.mPathMaxBounces;
        out << YAML::Key << "PathMaxSamples" << YAML::Value << settings.mRayTracing.mPathMaxSamples;

        // Persist display effects independently of the lighting technique.
        out << YAML::Key << "SsaoEnabled" << YAML::Value << settings.mSsaoEnabled;
        out << YAML::Key << "SsaoIntensity" << YAML::Value << settings.mSsaoIntensity;
        out << YAML::Key << "DepthOfFieldQuality" << YAML::Value << static_cast<unsigned>( settings.mDepthOfFieldQuality );
        out << YAML::Key << "BloomEnabled" << YAML::Value << settings.mBloom.mEnabled;
        out << YAML::Key << "BloomThreshold" << YAML::Value << settings.mBloom.mThreshold;
        out << YAML::Key << "BloomIntensity" << YAML::Value << settings.mBloom.mIntensity;
        out << YAML::Key << "BloomScatter" << YAML::Value << settings.mBloom.mScatter;
        out << YAML::Key << "BloomRadius" << YAML::Value << settings.mBloom.mRadius;
        out << YAML::Key << "ChromaticAberrationEnabled" << YAML::Value << settings.mChromaticAberration.mEnabled;
        out << YAML::Key << "ChromaticAberrationIntensity" << YAML::Value << settings.mChromaticAberration.mIntensity;
        out << YAML::Key << "ChromaticAberrationDirection" << YAML::Value << settings.mChromaticAberration.mDirection;

        out << YAML::Key << "VisualizeCascades" << YAML::Value << settings.mVisualizeCascades;
        out << YAML::Key << "SelectionOutlineWidth" << YAML::Value << settings.mSelectionOutlineWidth;
        out << YAML::Key << "SelectionOutlineColor" << YAML::Value << YAML::Flow << YAML::BeginSeq
            << settings.mSelectionOutlineColor.x << settings.mSelectionOutlineColor.y
            << settings.mSelectionOutlineColor.z << settings.mSelectionOutlineColor.w << YAML::EndSeq;
        out << YAML::Key << "VisualizeFrustum" << YAML::Value << settings.mVisualizeFrustum;
        out << YAML::EndMap;
    }

    MKT_NODISCARD static auto ReadRendererSettings( const YAML::Node& node ) -> renderer::SceneRenderSettings {
        renderer::SceneRenderSettings settings{};

        if ( !node || !node.IsMap() ) {
            return settings;
        }

        settings.mResolution = static_cast<renderer::rhi::RenderResolution>( node["Resolution"].as<unsigned>( 1 ) );
        settings.mShadowQuality = static_cast<renderer::ShadowMapQuality>( node["ShadowQuality"].as<unsigned>( static_cast<unsigned>( settings.mShadowQuality ) ) );
        settings.mShadowDistance = node["ShadowDistance"].as<float>( settings.mShadowDistance );
        settings.mCascadedShadowsEnabled = node["CascadedShadows"].as<bool>( settings.mCascadedShadowsEnabled );
        settings.mSoftShadowsEnabled = node["SoftShadows"].as<bool>( settings.mSoftShadowsEnabled );
        const auto samples{ node["Samples"].as<unsigned>( 1 ) };

        for ( unsigned index{}; index <= core::as<unsigned>( renderer::rhi::Multisampling::eMsaaX16 ); ++index ) {
            if ( samples == ( 1u << index ) ) {
                settings.mMultisampling = static_cast<renderer::rhi::Multisampling>( index );
                break;
            }
        }

        // Missing keys retain current defaults so older projects keep valid tracing budgets.
        settings.mRayTracing.mPathTracingEnabled = node["PathTracing"].as<bool>( settings.mRayTracing.mPathTracingEnabled );
        settings.mRayTracing.mShadowsEnabled = node["RayTracedShadows"].as<bool>( settings.mRayTracing.mShadowsEnabled );
        settings.mRayTracing.mReflectionsEnabled = node["RayTracedReflections"].as<bool>( settings.mRayTracing.mReflectionsEnabled );
        settings.mRayTracing.mGlobalIlluminationEnabled = node["RayTracedGlobalIllumination"].as<bool>( settings.mRayTracing.mGlobalIlluminationEnabled );
        settings.mRayTracing.mAmbientOcclusionEnabled = node["RayTracedAmbientOcclusion"].as<bool>( settings.mRayTracing.mAmbientOcclusionEnabled );
        settings.mRayTracing.mUseCosineWeightedAoSampling = node["AoCosineSampling"].as<bool>( settings.mRayTracing.mUseCosineWeightedAoSampling );
        settings.mRayTracing.mAoSamplesPerFrame = node["AoSamplesPerFrame"].as<unsigned>( settings.mRayTracing.mAoSamplesPerFrame );
        settings.mRayTracing.mAoMaxDistance = node["AoMaxDistance"].as<float>( settings.mRayTracing.mAoMaxDistance );
        settings.mRayTracing.mRayGiSamplesPerFrame = node["RayGiSamplesPerFrame"].as<unsigned>( settings.mRayTracing.mRayGiSamplesPerFrame );
        settings.mRayTracing.mRayGiMaxBounces = node["RayGiMaxBounces"].as<unsigned>( settings.mRayTracing.mRayGiMaxBounces );
        settings.mRayTracing.mPathSamplesPerFrame = node["PathSamplesPerFrame"].as<unsigned>( settings.mRayTracing.mPathSamplesPerFrame );
        settings.mRayTracing.mPathMaxBounces = node["PathMaxBounces"].as<unsigned>( settings.mRayTracing.mPathMaxBounces );
        settings.mRayTracing.mPathMaxSamples = node["PathMaxSamples"].as<unsigned>( settings.mRayTracing.mPathMaxSamples );

        settings.mSsaoEnabled = node["SsaoEnabled"].as<bool>( settings.mSsaoEnabled );
        settings.mSsaoIntensity = node["SsaoIntensity"].as<float>( settings.mSsaoIntensity );
        settings.mDepthOfFieldQuality = static_cast<renderer::DepthOfFieldQuality>( node["DepthOfFieldQuality"].as<unsigned>( static_cast<unsigned>( settings.mDepthOfFieldQuality ) ) );
        settings.mBloom.mEnabled = node["BloomEnabled"].as<bool>( settings.mBloom.mEnabled );
        settings.mBloom.mThreshold = node["BloomThreshold"].as<float>( settings.mBloom.mThreshold );
        settings.mBloom.mIntensity = node["BloomIntensity"].as<float>( settings.mBloom.mIntensity );
        settings.mBloom.mScatter = node["BloomScatter"].as<float>( settings.mBloom.mScatter );
        settings.mBloom.mRadius = node["BloomRadius"].as<float>( settings.mBloom.mRadius );
        settings.mChromaticAberration.mEnabled = node["ChromaticAberrationEnabled"].as<bool>( settings.mChromaticAberration.mEnabled );
        settings.mChromaticAberration.mIntensity = node["ChromaticAberrationIntensity"].as<float>( settings.mChromaticAberration.mIntensity );
        settings.mChromaticAberration.mDirection = node["ChromaticAberrationDirection"].as<float>( settings.mChromaticAberration.mDirection );

        settings.mVisualizeCascades = node["VisualizeCascades"].as<bool>( settings.mVisualizeCascades );
        settings.mSelectionOutlineWidth = node["SelectionOutlineWidth"].as<float>( settings.mSelectionOutlineWidth );

        if ( node["SelectionOutlineColor"] ) {
            const auto color{ node["SelectionOutlineColor"] };

            if ( color.IsSequence() && color.size() == 4 ) {
                settings.mSelectionOutlineColor.x = color[0].as<float>( settings.mSelectionOutlineColor.x );
                settings.mSelectionOutlineColor.y = color[1].as<float>( settings.mSelectionOutlineColor.y );
                settings.mSelectionOutlineColor.z = color[2].as<float>( settings.mSelectionOutlineColor.z );
                settings.mSelectionOutlineColor.w = color[3].as<float>( settings.mSelectionOutlineColor.w );
            }
        }

        settings.mVisualizeFrustum = node["VisualizeFrustum"].as<bool>( settings.mVisualizeFrustum );
        settings.Sanitize();
        return settings;
    }

    Project::Project( eastl::string_view name )
        : mName{ name } {
    }

    Project::Project( const ProjectCreateDescription& desc )
        : mName{ desc.mName },
          mAssetsRootDirectory{ desc.mAssetsRootDirectory },
          mAssetsRegistryDirectory{ desc.mAssetsRegistryDirectory },
          mCurrentScene{ desc.mCurrentScene } {
    }

    auto Project::SetLocation( const filesystem::Path& file ) -> void {
        MKT_BEGIN_PROFILER();
        const auto location{ file.ToAbsolute() };

        if ( mLocation == location ) {
            return;
        }

        mLocation = location;
        const auto root{ mLocation.GetPathTyped<std::filesystem::path>().parent_path() };
        mAssetsRootDirectory = filesystem::Path{ root / "Assets" };
        mAssetsRegistryDirectory = filesystem::Path{ root / "Assets" / ".cache" };
        BumpRevision();
    }

    auto Project::SetScenePath( const filesystem::Path& file ) -> void {
        MKT_BEGIN_PROFILER();
        const auto path{ file.IsEmpty() ? filesystem::Path{} : file.ToAbsolute() };

        if ( mScenePath != path ) {
            mScenePath = path;
            BumpRevision();
        }
    }

    auto Project::GetLocation() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mLocation;
    }

    auto Project::GetAssetsDirectory() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mAssetsRootDirectory;
    }

    auto Project::GetCacheDirectory() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mAssetsRegistryDirectory;
    }

    auto Project::GetScenePath() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mScenePath;
    }

    auto Project::SetMainCamera( const scene::Camera* camera ) -> void {
        // Attachment follows RestoreMainCamera; establish the loaded view's baseline.
        if ( !mMainCamera ) {
            mSerializedCameraSettings = CaptureCameraSettings( camera );
        }

        mMainCamera = camera;
    }

    auto Project::HasUnsavedChanges() const -> bool {
        return !IsRevision( mSerializedRevision ) ||
               ( mMainCamera && CaptureCameraSettings( mMainCamera ) != mSerializedCameraSettings );
    }

    auto Project::RestoreMainCamera( scene::Camera& camera ) const -> void {
        if ( auto* sceneCamera{ dynamic_cast<scene::SceneCamera*>( &camera ) } ) {
            std::ifstream input{ mLocation.GetPathTyped<std::filesystem::path>(), std::ios::binary };

            if ( !input ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open project camera settings." );
            }

            const auto document{ YAML::Load( input ) };
            DeserializeCameraSettings( *sceneCamera, document["EditorCamera"] );
        }
    }

    auto Project::Serialize( const filesystem::Path& filename ) const -> void {
        MKT_BEGIN_PROFILER();
        const auto cameraSettings{ CaptureCameraSettings( mMainCamera ) };
        const auto directory{ filename.ToAbsolute().GetPathTyped<std::filesystem::path>().parent_path() };
        const auto relative = [&]( const filesystem::Path& path ) {
            if ( path.IsEmpty() ) {
                return std::string{};
            }
            // Relative paths make the project folder portable. Cross-volume
            // references remain absolute, since no relative path exists.
            const auto absolute{ path.ToAbsolute().GetPathTyped<std::filesystem::path>() };
            const auto result{ absolute.lexically_relative( directory ) };
            return ( result.empty() ? absolute : result ).generic_string();
        };

        YAML::Emitter out;
        out << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "Project" << YAML::Value << mName.c_str();
        out << YAML::Key << "Assets" << YAML::Value << relative( mAssetsRootDirectory );
        out << YAML::Key << "Cache" << YAML::Value << relative( mAssetsRegistryDirectory );
        out << YAML::Key << "Scene" << YAML::Value << relative( mScenePath );
        out << YAML::Key << "CompressTextures" << YAML::Value << mCompressTextures;
        out << YAML::Key << "AutoSave" << YAML::Value << mIsAutoSaveEnabled;
        out << YAML::Key << "AutoSaveIntervalMs" << YAML::Value << mAutoSaveInterval;

        if ( const auto* camera{ dynamic_cast<const scene::SceneCamera*>( mMainCamera ) } ) {
            out << YAML::Key << "EditorCamera" << YAML::Value << SerializeCameraSettings( *camera );
        }

        WriteRendererSettings( out, mSceneRendererSettings );
        out << YAML::EndMap;
        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( filename ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open project file for writing." );
        }

        file->Write( out.c_str() );
        // Only a successful write acknowledges these settings as saved.
        mSerializedCameraSettings = cameraSettings;
        mSerializedRevision = GetRevision();
    }

    auto Project::Deserialize( const filesystem::Path& filename ) -> void {
        MKT_BEGIN_PROFILER();
        std::ifstream input{ filename.GetPathTyped<std::filesystem::path>(), std::ios::binary };

        if ( !input ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open project file." );
        }

        const auto document{ YAML::Load( input ) };

        if ( !document.IsMap() || !document["Project"] || document["Version"].as<core::u32>( 0 ) != 1 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid or unsupported Mikoto project." );
        }

        const auto name{ document["Project"].as<std::string>() };

        if ( name.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "The project name is empty." );
        }

        Project loaded{ name.c_str() };
        loaded.SetLocation( filename );
        const auto directory{ loaded.mLocation.GetPathTyped<std::filesystem::path>().parent_path() };
        const auto resolve = [&]( core::cstr key, core::cstr fallback ) {
            const auto value{ document[key].as<std::string>( fallback ) };
            return value.empty() ? filesystem::Path{} : filesystem::Path{ ( directory / std::filesystem::u8path( value ) ).lexically_normal() };
        };
        loaded.mAssetsRootDirectory = resolve( "Assets", "Assets" );
        loaded.mAssetsRegistryDirectory = resolve( "Cache", "Assets/.cache" );
        loaded.mScenePath = resolve( "Scene", "" );

        if ( loaded.mAssetsRootDirectory.IsEmpty() || loaded.mAssetsRegistryDirectory.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Project asset and cache paths cannot be empty." );
        }

        loaded.mCompressTextures = document["CompressTextures"] ? document["CompressTextures"].as<bool>() : false;
        loaded.mIsAutoSaveEnabled = document["AutoSave"].as<bool>( false );
        loaded.SetAutoSaveInterval( document["AutoSaveIntervalMs"].as<core::f32>( 2000 ) );
        loaded.mSceneRendererSettings = ReadRendererSettings( document["Renderer"] );

        // Commit only validated fields, so a
        // malformed project never partially replaces the current workspace.
        auto& project{ *this };
        project.mName = eastl::move( loaded.mName );
        project.mLocation = eastl::move( loaded.mLocation );
        project.mAssetsRootDirectory = eastl::move( loaded.mAssetsRootDirectory );
        project.mAssetsRegistryDirectory = eastl::move( loaded.mAssetsRegistryDirectory );
        project.mScenePath = eastl::move( loaded.mScenePath );
        project.mCompressTextures = loaded.mCompressTextures;
        project.mIsAutoSaveEnabled = loaded.mIsAutoSaveEnabled;
        project.mAutoSaveInterval = loaded.mAutoSaveInterval;
        project.mSceneRendererSettings = loaded.mSceneRendererSettings;

        BumpRevision();

        mSerializedRevision = GetRevision();
        mSerializedCameraSettings.clear();
        mMainCamera = nullptr;
    }

    auto Project::Serialize( filesystem::FileHandle file ) const -> void {
        MKT_BEGIN_PROFILER();

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "No project file selected." );
        }

        Serialize( file->GetPath() );
    }

    auto Project::Deserialize( filesystem::FileHandle file ) -> void {
        MKT_BEGIN_PROFILER();

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "No project file selected." );
        }

        Deserialize( file->GetPath() );
    }

    auto Project::SetCompressTextures( bool enabled ) -> void {
        if ( mCompressTextures != enabled ) {
            mCompressTextures = enabled;
            BumpRevision();
        }
    }

    auto Project::GetCompressTextures() const -> bool {
        return mCompressTextures;
    }

    auto Project::SetName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !name.empty() && mName != name ) {
            mName = name;
            BumpRevision();
        }
    }

    auto Project::SetAutoSaveInterval( core::f32 milliseconds ) -> void {
        MKT_BEGIN_PROFILER();

        if ( milliseconds >= 1000.0f && mAutoSaveInterval != milliseconds ) {
            mAutoSaveInterval = milliseconds;
            BumpRevision();
        }
    }

    auto Project::SetEnableSaveInterval( bool enable ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsAutoSaveEnabled != enable ) {
            mIsAutoSaveEnabled = enable;
            BumpRevision();
        }
    }

    auto Project::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto Project::GetScriptProjectPath() const -> filesystem::Path {
        const auto assets{ mAssetsRootDirectory.GetPathTyped<std::filesystem::path>() };
        const auto directory{ assets / "Scripts" / "CSharp" };
        // Existing project files and their references remain user-owned.
        for ( const auto& root: { directory, assets } ) {
            if ( !std::filesystem::exists( root ) ) {
                continue;
            }

            filesystem::Path project{};

            for ( const auto& entry: std::filesystem::directory_iterator{ root } ) {
                if ( entry.is_regular_file() && entry.path().extension() == ".csproj" ) {
                    if ( !project.IsEmpty() ) {
                        MKT_THROW_RUNTIME_ERROR( "Keep each C# project in a separate directory." );
                    }

                    project = filesystem::Path{ entry.path() };
                }
            }

            if ( !project.IsEmpty() ) {
                return project;
            }
        }

        return filesystem::Path{ directory / "Mikoto.Scripts.csproj" };
    }

    auto Project::EnsureScriptProject( const filesystem::Path& scriptCore ) const -> filesystem::Path {
        if ( mLocation.IsEmpty() ) {
            return {};
        }

        const auto assets{ mAssetsRootDirectory.GetPathTyped<std::filesystem::path>() };

        for ( const auto* language: { "Native", "Lua", "CSharp" } ) {
            std::filesystem::create_directories( assets / "Scripts" / language );
        }

        if ( !scriptCore.Exists() ) {
            return {};
        }

        const auto xml{ []( eastl::string_view value ) {
            eastl::string escaped{};

            for ( const auto character: value ) {
                switch ( character ) {
                    case '&':
                        escaped += "&amp;";
                        break;
                    case '<':
                        escaped += "&lt;";
                        break;
                    case '>':
                        escaped += "&gt;";
                        break;
                    case '"':
                        escaped += "&quot;";
                        break;
                    default:
                        escaped += character;
                        break;
                }
            }

            return escaped;
        } };

        const auto scripts{ GetScriptProjectPath() };

        // The project remains editable; only this machine-specific SDK reference
        // is regenerated so moving the editor does not leave a stale HintPath.
        const filesystem::Path properties{ scripts.GetPathTyped<std::filesystem::path>().parent_path() / "Mikoto.ScriptCore.generated.props" };
        const auto assembly{ xml( scriptCore.GetAbsolute() ) };
        const auto templateDirectory{ scriptCore.GetPathTyped<std::filesystem::path>().parent_path() };
        const auto loadTemplate{ [&]( core::cstr name ) -> eastl::string {
            const filesystem::Path path{ templateDirectory / name };
            return filesystem::FileHandle::New( path )->GetContentsString();
        } };
        const auto substitute{ []( eastl::string& text, eastl::string_view token, eastl::string_view value ) {
            core::usize offset{};

            while ( ( offset = text.find( token.data(), offset, token.size() ) ) != eastl::string::npos ) {
                text.replace( offset, token.size(), value.data(), value.size() );
                offset += value.size();
            }
        } };

#if defined( MIKOTO_ARCH_ARM64 )
        constexpr auto architecture{ "ARM64" };
#else
        constexpr auto architecture{ "x64" };
#endif
        // Scripts execute inside the native editor, so their target must match
        // its architecture. Keep this in regenerated properties so existing
        // game projects receive the fix without rewriting their .csproj files.
        auto propertiesText{ loadTemplate( "Mikoto.ScriptCore.generated.props.in" ) };
        substitute( propertiesText, "@PLATFORM_TARGET@", architecture );
        substitute( propertiesText, "@SCRIPT_CORE_PATH@", assembly );
        filesystem::FileHandle propertiesFile{ filesystem::FileService::Get()->CreateNewFile( properties ) };

        if ( propertiesFile.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot create script project properties." );
        }

        propertiesFile->Write( propertiesText );

        if ( !scripts.Exists() ) {
            filesystem::FileHandle scriptFile{ filesystem::FileService::Get()->CreateNewFile( scripts ) };

            if ( scriptFile.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create script project file." );
            }

            scriptFile->Write( loadTemplate( "Mikoto.Scripts.csproj.in" ) );
        }

        const auto root{ mLocation.GetPathTyped<std::filesystem::path>().parent_path() };
        const filesystem::Path solution{ root / "Mikoto.Scripts.slnx" };

        if ( !solution.Exists() ) {
            const filesystem::Path relative{ std::filesystem::relative( scripts.GetPathTyped<std::filesystem::path>(), root ) };
            auto solutionText{ loadTemplate( "Mikoto.Scripts.slnx.in" ) };
            substitute( solutionText, "@SCRIPT_PROJECT_PATH@", xml( relative.GetPath() ) );
            filesystem::FileHandle solutionFile{ filesystem::FileService::Get()->CreateNewFile( solution ) };

            if ( solutionFile.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create script solution file." );
            }

            solutionFile->Write( solutionText );
        }

        return solution;
    }

    auto Project::SetSceneRendererSettings( renderer::SceneRenderSettings settings ) -> void {
        settings.Sanitize();
        // Compare the fields written by WriteRendererSettings, excluding transient
        // renderer diagnostics. Reapplying unchanged settings must not dirty a project.
        const auto persistentFields{ []( const renderer::SceneRenderSettings& value ) {
            return std::tie( value.mResolution, value.mMultisampling, value.mShadowQuality,
                value.mShadowDistance, value.mCascadedShadowsEnabled, value.mSoftShadowsEnabled, value.mRayTracing.mPathTracingEnabled,
                value.mRayTracing.mShadowsEnabled, value.mRayTracing.mReflectionsEnabled,
                value.mRayTracing.mGlobalIlluminationEnabled, value.mRayTracing.mAmbientOcclusionEnabled,
                value.mRayTracing.mUseCosineWeightedAoSampling, value.mRayTracing.mAoSamplesPerFrame, value.mRayTracing.mAoMaxDistance,
                value.mRayTracing.mRayGiSamplesPerFrame,
                value.mRayTracing.mRayGiMaxBounces, value.mRayTracing.mPathSamplesPerFrame,
                value.mRayTracing.mPathMaxBounces, value.mRayTracing.mPathMaxSamples,
                value.mSsaoEnabled, value.mSsaoIntensity, value.mDepthOfFieldQuality,
                value.mBloom.mEnabled, value.mBloom.mThreshold, value.mBloom.mIntensity,
                value.mBloom.mScatter, value.mBloom.mRadius, value.mChromaticAberration.mEnabled,
                value.mChromaticAberration.mIntensity, value.mChromaticAberration.mDirection,
                value.mVisualizeCascades, value.mSelectionOutlineWidth, value.mSelectionOutlineColor,
                value.mVisualizeFrustum );
        } };

        if ( persistentFields( mSceneRendererSettings ) != persistentFields( settings ) ) {
            BumpRevision();
        }

        mSceneRendererSettings = settings;
    }

    auto Project::GetSceneRendererSettings() const -> const renderer::SceneRenderSettings& {
        return mSceneRendererSettings;
    }
}// namespace mikoto::project