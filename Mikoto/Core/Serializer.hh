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

        virtual auto Serialize(const SerializeObjT& obj, const Path& savePath) -> void = 0;
        virtual auto Deserialize(const Path& loadPath) -> Unique<SerializeObjT> = 0;
    };
}
#endif //SERIALIZER_HH
