//
// Created by zanet on 1/26/2025.
//

#ifndef SERIALIZER_HH
#define SERIALIZER_HH

#include <Library/Utility/Types.hh>

namespace Mikoto {
    template <typename SerializedObjT>
    class ISerializer {
    public:
        virtual ~ISerializer() = default;

        virtual auto Serialize(const SerializedObjT& obj, const Path& savePath) -> void = 0;
        virtual auto Deserialize(const Path& loadPath) -> Unique<SerializedObjT> = 0;
    };
}
#endif //SERIALIZER_HH
