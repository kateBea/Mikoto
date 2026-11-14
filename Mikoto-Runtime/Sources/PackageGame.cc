#include <fstream>

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Scene/Scene.hh>

#include <Application/PackageGame.hh>

namespace mikoto::runtime {
    namespace fs = std::filesystem;
    using namespace core;

    struct PackagePaths {
        fs::path mSource{};
        fs::path mOutput{};
        fs::path mResources{};
        fs::path mCache{};
        fs::path mStartup{};
    };

    struct PackagedScene {
        fs::path mDestination{};
        YAML::Node mDocument{};
    };

    MKT_NODISCARD static auto Utf8( const fs::path& path ) -> eastl::string {
        const auto bytes{ path.generic_u8string() };
        return { reinterpret_cast<const char*>( bytes.data() ), bytes.size() };
    }

    MKT_NODISCARD static auto IsInside( const fs::path& path, const fs::path& root ) -> bool {
        const auto relative{ fs::relative( path, root ) };
        return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
    }

    MKT_NODISCARD static auto ReadYaml( const fs::path& path ) -> YAML::Node {
        std::ifstream input{ path };

        if ( !input ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot read package input: {}", Utf8( path ) ) );
        }

        return YAML::Load( input );
    }

    static auto WriteYaml( const fs::path& path, const YAML::Node& document ) -> void {
        std::ofstream output{ path };
        output << document;
        output.close();

        if ( !output ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot write package file: {}", Utf8( path ) ) );
        }
    }

    MKT_NODISCARD static auto RelocateAsset( const fs::path& asset, const PackagePaths& paths ) -> fs::path {
        if ( IsInside( asset, paths.mSource ) ) {
            return paths.mOutput / "Game" / fs::relative( asset, paths.mSource );
        }

        if ( IsInside( asset, paths.mResources ) ) {
            return paths.mOutput / "Resources" / fs::relative( asset, paths.mResources );
        }

        MKT_THROW_RUNTIME_ERROR( string::Format( "Import this external asset into the project before shipping: {}", Utf8( asset ) ) );
    }

    static auto RewriteSceneAssets( YAML::Node node, const PackagePaths& paths, const fs::path& destination,
        core::usize& visited, core::u32 depth = 0 ) -> void {
        constexpr core::u32 kMaximumDepth{ 256 };
        constexpr core::usize kMaximumNodes{ 1000000 };

        if ( depth > kMaximumDepth || ++visited > kMaximumNodes ) {
            MKT_THROW_RUNTIME_ERROR( "Scene document is too deeply nested or too large." );
        }

        if ( node.IsSequence() ) {
            for ( auto child: node ) {
                RewriteSceneAssets( child, paths, destination, visited, depth + 1 );
            }
        }
        else if ( node.IsMap() ) {
            for ( auto entry: node ) {
                const auto& key{ entry.first.Scalar() };
                const bool assetPath{ key == "MaterialPath" || key == "ModelPath" || key == "TexturePath" || key == "FontPath" ||
                                      key == "ClipPath" || key == "ScriptPath" };

                if ( !assetPath ) {
                    RewriteSceneAssets( entry.second, paths, destination, visited, depth + 1 );
                    continue;
                }

                if ( !entry.second.IsScalar() || entry.second.Scalar().empty() ) {
                    continue;
                }

                const auto asset{ fs::canonical( fs::u8path( entry.second.Scalar() ) ) };
                const auto target{ RelocateAsset( asset, paths ) };
                node[key] = Utf8( target.lexically_relative( destination.parent_path() ) ).c_str();
            }
        }
    }

    static auto CollectProjectFiles( const PackagePaths& paths, eastl::vector<fs::path>& files,
        eastl::vector<PackagedScene>& scenes ) -> void {
        for ( auto it{ fs::recursive_directory_iterator{ paths.mSource } }; it != fs::recursive_directory_iterator{}; ++it ) {
            const auto name{ it->path().filename() };

            if ( it->is_symlink() ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Package input contains a symbolic link: {}", Utf8( it->path() ) ) );
            }

            if ( it->is_directory() ) {
                if ( name == ".git" || name == ".idea" || name == ".vs" || name == ".cache" ||
                     fs::weakly_canonical( it->path() ) == paths.mCache ) {
                    it.disable_recursion_pending();
                }

                continue;
            }

            if ( !it->is_regular_file() ) {
                continue;
            }

            files.push_back( it->path() );

            if ( it->path().extension() != ".mktscn" && it->path().extension() != ".mktmtl" && it->path() != paths.mStartup ) {
                continue;
            }

            auto document{ ReadYaml( it->path() ) };
            const bool materialFile{ it->path().extension() == ".mktmtl" };

            if ( !materialFile || document["Version"].as<core::i32>( 1 ) >= 2 ) {
                scene::Scene::ConvertSceneAssetPaths( document, filesystem::Path{ it->path() }, false );
            }

            const auto destination{ paths.mOutput / "Game" / fs::relative( it->path(), paths.mSource ) };
            core::usize visited{};
            RewriteSceneAssets( document, paths, destination, visited );

            if ( materialFile ) {
                document["Version"] = 2;
            }

            scenes.push_back( PackagedScene{ destination, document } );
        }
    }

    static auto StageRuntime( const fs::path& executable, const PackagePaths& paths ) -> void {
        fs::copy_file( executable, paths.mOutput / executable.filename() );

        // Native plugins are deliberately staged beside the executable by CMake.
        for ( const auto& entry: fs::directory_iterator{ executable.parent_path() } ) {
            const auto extension{ entry.path().extension() };
            const auto name{ Utf8( entry.path().filename() ) };
            const bool library{ extension == ".dll" || extension == ".dylib" || name.find( ".so" ) != eastl::string::npos };

            if ( entry.is_regular_file() && library ) {
                fs::copy_file( entry.path(), paths.mOutput / entry.path().filename() );
            }
        }
        // Runtime DLLs and their redistribution notices travel together.
        const auto licenses{ executable.parent_path() / "Licenses" };

        if ( fs::is_directory( licenses ) ) {
            fs::copy( licenses, paths.mOutput / "Licenses", fs::copy_options::recursive );
        }

        fs::copy( paths.mResources, paths.mOutput / "Resources", fs::copy_options::recursive );
    }

    auto PackageGame( const project::Project& project, const fs::path& executable,
        const fs::path& resources, const fs::path& destination ) -> void {
        const auto projectFile{ fs::canonical( project.GetLocation().GetPathTyped<fs::path>() ) };
        const PackagePaths paths{
            .mSource = projectFile.parent_path(),
                .mOutput = fs::weakly_canonical( destination ),
                    .mResources = fs::canonical( resources ),
                        .mCache = fs::weakly_canonical( project.GetCacheDirectory().GetPathTyped<fs::path>() ),
                            .mStartup = fs::canonical( project.GetScenePath().GetPathTyped<fs::path>() )
        };

        if ( fs::exists( paths.mOutput ) || IsInside( paths.mOutput, paths.mSource ) || IsInside( paths.mOutput, paths.mResources ) ) {
            MKT_THROW_RUNTIME_ERROR( "Choose a new package directory outside the project and engine Resources directories." );
        }

        const auto assets{ fs::canonical( project.GetAssetsDirectory().GetPathTyped<fs::path>() ) };

        if ( !IsInside( paths.mStartup, paths.mSource ) || !IsInside( assets, paths.mSource ) ) {
            MKT_THROW_RUNTIME_ERROR( "Move the startup scene and Assets directory inside the project before shipping." );
        }

        // Validate and rewrite references before creating any output. Keep model
        // sidecars and script imports by preserving the project's relative layout.
        eastl::vector<fs::path> files{};
        eastl::vector<PackagedScene> scenes{};
        CollectProjectFiles( paths, files, scenes );

        auto document{ ReadYaml( projectFile ) };
        document["Scene"] = Utf8( fs::relative( paths.mStartup, paths.mSource ) ).c_str();
        document["Assets"] = Utf8( fs::relative( assets, paths.mSource ) ).c_str();
        document["Cache"] = "Assets/.cache";

        fs::create_directories( paths.mOutput / "Game" );
        StageRuntime( executable, paths );

        for ( const auto& file: files ) {
            const auto target{ paths.mOutput / "Game" / fs::relative( file, paths.mSource ) };
            fs::create_directories( target.parent_path() );
            fs::copy_file( file, target );
        }

        for ( const auto& scene: scenes ) {
            WriteYaml( scene.mDestination, scene.mDocument );
        }

        WriteYaml( paths.mOutput / "Game" / projectFile.filename(), document );

        YAML::Node manifest{};
        manifest["Project"] = Utf8( fs::path{ "Game" } / projectFile.filename() ).c_str();
        WriteYaml( paths.mOutput / "game.yaml", manifest );
    }
}// namespace mikoto::runtime
