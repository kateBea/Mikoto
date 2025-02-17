//
// Created by zanet on 1/26/2025.
//

#ifndef SERIALIZER_HH
#define SERIALIZER_HH

#include <Library/Utility/Types.hh>

namespace Mikoto {
    template <typename SerializeObjT>
    class ISerializer {
    public:
        virtual ~ISerializer() = default;

        virtual auto Serialize(const SerializeObjT& obj, const Path_T& savePath) -> void = 0;
        virtual auto Deserialize(const Path_T& loadPath) -> Scope_T<SerializeObjT> = 0;
    };
}
#endif //SERIALIZER_HH
