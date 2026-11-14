//    Copyright 2025 ケイト
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

#include <set>
#include <chrono>
#include <vector>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <exception>
#include <filesystem>

#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>

#include <slang-com-ptr.h>

#include <yaml-cpp/yaml.h>

#include <Core/DateTime.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Assets/ShaderLibrary.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#endif

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    // Content-addressed dependencies follow the same principle as compiler caches:
    // timestamps describe files; hashes decide whether their contents changed.
    // https://ccache.dev/manual/latest.html#_the_direct_mode
    // FNV-1a is used for local cache identity, not for security decisions.
    // https://www.ietf.org/archive/id/draft-eastlake-fnv-25.html
    static auto HashShaderBytes( const void* data, usize size ) -> std::string {
        u64 hash{ 14695981039346656037ull };
        const auto* bytes{ static_cast<const unsigned char*>( data ) };
        for ( usize i{}; i < size; ++i ) {
            hash = ( hash ^ bytes[i] ) * 1099511628211ull;
        }
        std::ostringstream text{};
        text << std::hex << std::setw( 16 ) << std::setfill( '0' ) << hash;
        return text.str();
    }

    static auto ReadShaderBytes( const std::filesystem::path& path ) -> std::vector<u32> {
        std::ifstream file{ path, std::ios::binary | std::ios::ate };
        if ( !file ) {
            return {};
        }
        const auto size{ file.tellg() };
        if ( size <= 0 || size % 4 != 0 ) {
            return {};
        }
        std::vector<u32> bytes( static_cast<size_t>( size ) / 4 );
        file.seekg( 0 );
        file.read( reinterpret_cast<char*>( bytes.data() ), size );
        return file ? bytes : std::vector<u32>{};
    }

    static auto ShaderDependency( const std::filesystem::path& path ) -> YAML::Node {
        std::ifstream file{ path, std::ios::binary };
        if ( !file ) {
            throw std::runtime_error{ "Cannot read shader dependency." };
        }
        const std::string bytes{ std::istreambuf_iterator<char>{ file }, {} };

        // File clocks have platform-specific epochs; serialize an explicit UTC date.
        // https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class
        const auto modified{ std::filesystem::last_write_time( path ) };
#if defined( _MSC_VER )
        const auto systemTime{ std::chrono::clock_cast<std::chrono::system_clock>( modified ) };
#else
        const auto systemTime{ std::chrono::file_clock::to_sys( modified ) };
#endif
        const auto timestamp{ DateTime::FromSystemTime( std::chrono::floor<DateTime::Duration>( systemTime ) )
            .ToString( "yyyy-MM-dd'T'HH:mm:ss.fffK" ) };

        YAML::Node node{};
        node["Path"] = std::filesystem::absolute( path ).lexically_normal().generic_string();
        node["LastWriteTime"] = timestamp.c_str();
        node["Size"] = bytes.size();
        node["Hash"] = HashShaderBytes( bytes.data(), bytes.size() );
        return node;
    }

    static auto LoadShaderCache( const std::filesystem::path& metadataPath, const std::string& identity,
        ShaderLanguage language ) -> std::vector<u32> {
        try {
            const auto node{ YAML::LoadFile( metadataPath.string() ) };
            if ( node["Version"].as<int>() != 1 || node["Identity"].as<std::string>() != identity ||
                 !node["Dependencies"].IsSequence() || node["Dependencies"].size() == 0 ) {
                return {};
            }
            for ( const auto& recorded: node["Dependencies"] ) {
                const auto current{ ShaderDependency( recorded["Path"].as<std::string>() ) };
                if ( current["Hash"].as<std::string>() != recorded["Hash"].as<std::string>() ) {
                    return {};
                }
            }
            auto binaryPath{ metadataPath };
            binaryPath.replace_extension( language == ShaderLanguage::eSPIRV ? ".spv" : ".dxil" );
            auto bytes{ ReadShaderBytes( binaryPath ) };
            if ( bytes.empty() || HashShaderBytes( bytes.data(), bytes.size() * 4 ) != node["BytecodeHash"].as<std::string>() ) {
                return {};
            }
            if ( language == ShaderLanguage::eSPIRV && ( bytes.size() < 5 || bytes[0] != 0x07230203u ) ) {
                return {};
            }
            return bytes;
        } catch ( const std::exception& ) {
            // A missing, stale or interrupted cache is a normal compilation miss.
            return {};
        }
    }

    static auto PublishShaderCache( const std::filesystem::path& metadataPath, YAML::Node metadata,
        ShaderLanguage language, const IShaderModule& module ) -> void {
        // Publish bytecode first, metadata last; readers verify the binary hash.
        // Unique temporary paths avoid exposing incomplete files to other processes.
        auto binaryPath{ metadataPath };
        binaryPath.replace_extension( language == ShaderLanguage::eSPIRV ? ".spv" : ".dxil" );
        std::filesystem::create_directories( metadataPath.parent_path() );
        metadata["BytecodeHash"] = HashShaderBytes( module.GetContents(), module.GetContentsByteSize() );
        const std::string suffix{ ".tmp-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) };
        auto publish{ [&]( const std::filesystem::path& path, const void* data, size_t size ) {
            const auto temporary{ std::filesystem::path{ path.string() + suffix } };
            std::ofstream file{ temporary, std::ios::binary | std::ios::trunc };
            file.write( static_cast<const char*>( data ), static_cast<std::streamsize>( size ) );
            file.close();
            if ( !file ) {
                std::filesystem::remove( temporary );
                return;
            }
            std::error_code error{};
            std::filesystem::remove( path, error );
            error.clear();
            std::filesystem::rename( temporary, path, error );
            if ( error ) {
                std::filesystem::remove( temporary );
            }
        } };
        publish( binaryPath, module.GetContents(), module.GetContentsByteSize() );
        const auto text{ YAML::Dump( metadata ) };
        publish( metadataPath, text.data(), text.size() );
    }

    static auto CompileLibraryShader( slang::ISession& session, const filesystem::Path& path,
        ShaderType stage, eastl::string_view entryPoint, GraphicsAPI api, YAML::Node dependencies,
        eastl::string_view debugSymbolsDirectory ) -> std::vector<u32> {
        // Ask Slang for dependencies, including imported modules; do not parse
        // #include/import ourselves. Only cache misses enter the compilation API.
        // https://docs.shader-slang.org/en/latest/compilation-api.html
        Slang::ComPtr<slang::IBlob> diagnostics{};
        Slang::ComPtr<slang::IModule> module{ session.loadModuleFromSource(
            path.GetPathTyped<std::filesystem::path>().stem().string().c_str(), path.GetC_Str(), nullptr, diagnostics.writeRef() ) };

        if ( !module ) {
            MKT_THROW_RUNTIME_ERROR( diagnostics ? static_cast<const char*>( diagnostics->getBufferPointer() ) : "Cannot load shader source." );
        }

        Slang::ComPtr<slang::IEntryPoint> entry{};
        Slang::ComPtr<slang::IComponentType> program{}, linked{};
        Slang::ComPtr<slang::IBlob> code{};

        if ( !SLANG_SUCCEEDED( module->findEntryPointByName( entryPoint.data(), entry.writeRef() ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot find shader entry point." );
        }

        slang::IComponentType* components[]{ module, entry };
        if ( !SLANG_SUCCEEDED( session.createCompositeComponentType( components, 2, program.writeRef(), diagnostics.writeRef() ) ) ||
             !SLANG_SUCCEEDED( program->link( linked.writeRef(), diagnostics.writeRef() ) ) ||
             !SLANG_SUCCEEDED( linked->getEntryPointCode( 0, 0, code.writeRef(), diagnostics.writeRef() ) ) || !code ) {
            MKT_THROW_RUNTIME_ERROR( diagnostics ? static_cast<const char*>( diagnostics->getBufferPointer() ) : "Cannot compile shader." );
        }

        // Walk this module's imports, keeping unrelated shaders out of its manifest.
        // Older Slang versions without the import-query interface use the conservative
        // session dependency list; source invalidation remains correct there too.
        std::set<std::string> paths{ path.GetAbsolute().c_str() };
        std::vector<Slang::ComPtr<slang::IModule>> pending{ module };
        std::set<slang::IModule*> visited{};
        bool queriedImports{ true };

        while ( !pending.empty() ) {
            const auto dependency{ pending.back() };
            pending.pop_back();
            if ( !visited.insert( dependency.get() ).second ) {
                continue;
            }

            for ( SlangInt32 i{}; i < dependency->getDependencyFileCount(); ++i ) {
                paths.insert( dependency->getDependencyFilePath( i ) );
            }

            Slang::ComPtr<slang::IModulePrecompileService_Experimental> imports{};
            if ( !SLANG_SUCCEEDED( dependency->queryInterface( slang::IModulePrecompileService_Experimental::getTypeGuid(), reinterpret_cast<void**>( imports.writeRef() ) ) ) ) {
                queriedImports = false;
                break;
            }

            for ( SlangInt i{}; i < imports->getModuleDependencyCount(); ++i ) {
                Slang::ComPtr<slang::IModule> imported{};
                if ( SLANG_SUCCEEDED( imports->getModuleDependency( i, imported.writeRef() ) ) && imported ) {
                    pending.push_back( imported );
                }
            }
        }

        if ( !queriedImports ) {
            for ( SlangInt index{}; index < session.getLoadedModuleCount(); ++index ) {
                auto* dependency{ session.getLoadedModule( index ) };
                for ( SlangInt32 i{}; i < dependency->getDependencyFileCount(); ++i ) {
                    paths.insert( dependency->getDependencyFilePath( i ) );
                }
            }
        }

        for ( const auto& source: paths ) {
            dependencies.push_back( ShaderDependency( source ) );
        }

#if defined( MIKOTO_PLATFORM_WINDOWS )
        // Slang emits HLSL for this backend; finish the existing DXC policy here
        // so the shader object always receives native DXIL, including cache misses.
        if ( api == GraphicsAPI::eD3D12 ) {
            auto* context{ checked_cast<renderer::d3d12::Context*>( renderer::RenderSystem::Get()->GetContext() ) };
            const auto description{ renderer::d3d12::ShaderCompileDescription{}
                .SetSource( eastl::string_view{ static_cast<const char*>( code->getBufferPointer() ), code->getBufferSize() } )
                .SetSourceName( path.GetC_Str() )
                .SetDebugSymbolsDirectory( debugSymbolsDirectory )
                .SetEntryPoint( entryPoint )
                .SetProfile( stage )
#if !defined( NDEBUG )
                .EnableDebug()
#else
                .StripReflection()
#endif
                .WarningsAsErrors() };
            const auto native{ context->GetShaderCompiler()->Compile( description ) };
            std::vector<u32> bytes( ( native->GetBufferSize() + 3 ) / 4 );
            std::memcpy( bytes.data(), native->GetBufferPointer(), native->GetBufferSize() );
            return bytes;
        }
#endif
        std::vector<u32> bytes( ( code->getBufferSize() + 3 ) / 4 );
        std::memcpy( bytes.data(), code->getBufferPointer(), code->getBufferSize() );
        return bytes;
    }

    ShaderLibrary::ShaderLibrary( const ShaderLibraryDescription& options )
        : mDevice{ options.mDevice }, mRootPath{ options.mRootPath }, mCacheShaders{ options.mCacheShaders }, mCachePath{ options.mCachePath } {
    }

    auto ShaderLibrary::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        const std::lock_guard lock{ mShaderMutex };
        mStopping = false;
        mIsInitialized = true;
    }

    auto ShaderLibrary::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        std::unique_lock lock{ mShaderMutex };
        mStopping = true;
        mCompilationsFinished.wait( lock, [this] {
            return mPendingCompilations == 0;
        } );
        mShaders.clear();
        mIsInitialized = false;
    }

    ShaderLibrary::~ShaderLibrary() {
        Shutdown();
    }

    auto ShaderLibrary::GetShader( eastl::string_view uri, renderer::rhi::ShaderType type, eastl::string_view entryPoint ) -> renderer::rhi::ShaderModuleHandle {
        MKT_BEGIN_PROFILER();
        const auto path{ filesystem::PathBuilder{}.SetPath( mRootPath ).SetPath( uri.data() ).Build() };
        std::shared_future<ShaderModuleHandle> result{};
        {
            const std::lock_guard lock{ mShaderMutex };

            if ( const auto found{ mShaders.find( path ) }; found != mShaders.end() ) {
                for ( const auto& variant: found->second ) {
                    if ( variant.mStage == type && variant.mEntryPoint == entryPoint ) {
                        result = variant.mResult;
                        break;
                    }
                }
            }
        }

        if ( result.valid() && result.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
            try {
                return result.get();
            } catch ( ... ) {
                // A failed compilation is removed so LoadShader() can retry.
            }
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::LoadShader( eastl::string_view uri, renderer::rhi::ShaderType type, eastl::string_view entryPoint ) -> renderer::rhi::ShaderModuleHandle {
        MKT_BEGIN_PROFILER_NAMED();
        const auto path{ filesystem::PathBuilder{}.SetPath( mRootPath ).SetPath( uri.data() ).Build() };
        std::promise<ShaderModuleHandle> compilation{};
        std::shared_future<ShaderModuleHandle> result{};
        bool ownsCompilation{};
        {
            const std::lock_guard lock{ mShaderMutex };

            if ( mStopping ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot load a shader from a stopped shader library." );
            }

            auto& variants{ mShaders[path] };

            for ( const auto& variant: variants ) {
                if ( variant.mStage == type && variant.mEntryPoint == entryPoint ) {
                    result = variant.mResult;
                    break;
                }
            }

            if ( !result.valid() ) {
                result = compilation.get_future().share();
                variants.push_back( { eastl::string{ entryPoint }, type, result } );
                ++mPendingCompilations;
                ownsCompilation = true;
            }
        }

        // Wait without a cache lock or a Slang lease. The owner compiles once.
        if ( !ownsCompilation ) {
            return result.get();
        }

        ShaderModuleHandle module{};
        std::exception_ptr failure{};

        try {
            const auto api{ mDevice->GetGraphicsApi() };
            if ( api != GraphicsAPI::eVulkan && api != GraphicsAPI::eD3D12 ) {
                const auto source{ filesystem::FileService::Get()->LoadFile( path ) };
                if ( !source ) {
                    MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot read shader source: {}", path.GetPath() ) );
                }
                module = mDevice->CreateShader( ShaderModuleCreateDescription{}
                    .SetContents( source )
                    .SetModuleName( source->GetName() )
                    .SetModulePath( source->GetPath() )
                    .SetLanguage( ShaderLanguage::eSlang )
                    .SetStage( type )
                    .SetEntryPoint( entryPoint ) );
            }
            else {
                const auto binaryLanguage{ api == GraphicsAPI::eVulkan ? ShaderLanguage::eSPIRV : ShaderLanguage::eDXIL };
                const bool persistent{ mCacheShaders && ( api == GraphicsAPI::eVulkan || api == GraphicsAPI::eD3D12 ) };
                auto lease{ renderer::RenderSystem::Get().AcquireSlangSession() };
                const auto compilerKey{ lease.GetCacheKey( type ) };
                const auto absolute{ std::filesystem::absolute( path.GetPathTyped<std::filesystem::path>() ).lexically_normal() };
                // Recompile Debug DXIL that embedded ILDB; symbols now live in
                // separate PDBs. Release and Vulkan keep their existing cache keys.
#if !defined( NDEBUG )
                const auto dxcPolicy{ api == GraphicsAPI::eD3D12 ? "dxc-sm68-debug-pdb-v2" : "dxc-sm68-debug-strip-v1" };
#else
                constexpr auto dxcPolicy{ "dxc-sm68-debug-strip-v1" };
#endif
                const auto identity{ string::Format( "mktshader-1|{}|{}|{}|{}|{}|{}",
                    absolute.generic_string(), static_cast<u32>( api ), static_cast<u32>( type ), entryPoint, compilerKey, dxcPolicy ) };
                const auto metadataPath{ mCachePath.GetPathTyped<std::filesystem::path>() /
                                         ( HashShaderBytes( identity.data(), identity.size() ) + ".mktshader" ) };
                auto bytes{ persistent ? LoadShaderCache( metadataPath, identity.c_str(), binaryLanguage ) : std::vector<u32>{} };
                YAML::Node metadata{};
                const bool cacheHit{ !bytes.empty() };
                if ( !cacheHit ) {
                    bytes = CompileLibraryShader( *lease.GetSession( type ), filesystem::Path{ absolute }, type, entryPoint, api,
                        metadata["Dependencies"], mCachePath.GetPath() );
                }
                const auto description{ ShaderModuleCreateDescription{}
                    .SetContents( bytes.data(), bytes.size() * sizeof( u32 ) )
                    .SetModuleName( absolute.stem().string().c_str() )
                    .SetModulePath( absolute.generic_string().c_str() )
                    .SetLanguage( binaryLanguage )
                    .SetStage( type )
                    .SetEntryPoint( entryPoint ) };
                module = mDevice->CreateShader( description );
                if ( persistent && !cacheHit && module ) {
                    metadata["Version"] = 1;
                    metadata["Identity"] = identity.c_str();
                    try {
                        PublishShaderCache( metadataPath, metadata, binaryLanguage, *module );
                    } catch ( const std::exception& error ) {
                        MKT_CORE_LOGGER_WARN( "Cannot store shader cache '{}': {}", metadataPath.string(), error.what() );
                    }
                }
            }
            compilation.set_value( module );
        } catch ( ... ) {
            failure = std::current_exception();
            compilation.set_exception( failure );
        }

        {
            const std::lock_guard lock{ mShaderMutex };

            if ( failure || !module ) {
                // Every waiter observes the same failure. A later request can retry.
                const auto found{ mShaders.find( path ) };

                if ( found != mShaders.end() ) {
                    eastl::erase_if( found->second, [&]( const ShaderVariant& variant ) {
                        return variant.mStage == type && variant.mEntryPoint == entryPoint;
                    } );

                    if ( found->second.empty() ) {
                        mShaders.erase( found );
                    }
                }
            }

            --mPendingCompilations;
            mCompilationsFinished.notify_all();
        }

        return result.get();
    }
}// namespace mikoto::asset
