//
// Created by zanet on 1/27/2025.
//

#ifndef SHADERLIBRARY_HH
#define SHADERLIBRARY_HH

#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/GpuDevice.hh>

namespace Mikoto {
    struct ShaderLibraryDescription {
        Path_T FilePath{};
        GpuDevice* Device{ nullptr };
    };

    class ShaderLibrary final : public IService<ShaderLibrary>{
    public:
        explicit ShaderLibrary( const ShaderLibraryDescription &options );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto GetShader( std::string_view uri ) -> ShaderModuleHandle;
        auto LoadShader( const ShaderModuleDescription &loadInfo ) -> ShaderModuleHandle;

    private:
        GpuDevice* m_Device{ nullptr };
        ankerl::unordered_dense::map<std::string, ShaderModuleHandle> m_Shaders{};
    };

}


#endif // SHADERLIBRARY_HH
