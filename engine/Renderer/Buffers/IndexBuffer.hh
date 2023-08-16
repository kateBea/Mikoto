/**
 * IndexBuffer.hh
 * Created by kate on 6/5/23.
 * */

#ifndef MIKOTO_INDEX_BUFFER_HH
#define MIKOTO_INDEX_BUFFER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    /**
     * General interface for Vertex index buffers
     * */
     class IndexBuffer {
     public:
         virtual ~IndexBuffer() = default;

         KT_NODISCARD virtual auto GetCount() const -> UInt32_T { return m_Count; }

         static auto Create(const std::vector<UInt32_T>& data) -> std::shared_ptr<IndexBuffer>;
     protected:
         explicit IndexBuffer(UInt32_T count = 0) : m_Count{ count } {}

     protected:
         UInt32_T m_Count{};
     };
}

#endif // MIKOTO_INDEX_BUFFER_HH
