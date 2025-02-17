//
// Created by zanet on 1/28/2025.
//

#ifndef PROJECTSERIALIZER_HH
#define PROJECTSERIALIZER_HH

#include <Project/Project.hh>
#include <Core/Serialization/Serializer.hh>

namespace Mikoto {
    class ProjectSerializer final : public ISerializer<Project> {
    public:
        auto Serialize( const Project& obj, const Path_T& savePath ) -> void override;
        auto Deserialize( const Path_T& loadPath ) -> Scope_T<Project> override;
    };
}


#endif//PROJECTSERIALIZER_HH
