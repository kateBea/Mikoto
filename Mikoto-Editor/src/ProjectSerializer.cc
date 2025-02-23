//
// Created by zanet on 1/28/2025.
//

#include <Project/ProjectSerializer.hh>

namespace Mikoto {


    auto ProjectSerializer::Serialize( const Project &obj, const Path_T &savePath ) -> void {
    }

    auto ProjectSerializer::Deserialize( const Path_T &loadPath ) -> Scope_T<Project> {
        return {};
    }
}// namespace Mikoto
