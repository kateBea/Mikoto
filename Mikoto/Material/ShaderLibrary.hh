//
// Created by zanet on 1/27/2025.
//

#ifndef SHADERLIBRARY_HH
#define SHADERLIBRARY_HH

#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Material/ShaderModule.hh>

namespace Mikoto {

    struct ShaderLibraryDescription {
        Path RootPath{};
        GpuDevice* Device{ nullptr };
    };

    // The shader library keeps track of the shaders loaded from the disk
    class ShaderLibrary final : public IService, public Singleton<ShaderLibrary> {
    public:
        explicit ShaderLibrary( const ShaderLibraryDescription &options );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto GetShader( std::string_view uri ) -> ShaderModuleHandle;
        auto LoadShader( const ShaderModuleDescription &loadInfo ) -> ShaderModuleHandle;
        auto LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle;

        ~ShaderLibrary() override = default;

    private:
        Path m_RootPath{};
        GpuDevice *m_Device{ nullptr };
        ankerl::unordered_dense::map<std::string, ShaderModuleHandle> m_Shaders{};
    };

}// namespace Mikoto


#endif // SHADERLIBRARY_HH
