//
// Created by kate on 6/4/23.
//

#ifndef KATE_ENGINE_INDEX_BUFFER_HH
#define KATE_ENGINE_INDEX_BUFFER_HH

#include <memory>

#include <Utility/Common.hh>

namespace kaTe {
    /**
     * General interface for Vertex index buffers
     * */
     class IndexBuffer {
     public:
         IndexBuffer() = default;
         virtual ~IndexBuffer() = default;

         KT_NODISCARD virtual auto GetID() const -> UInt32_T { return m_Id; }
         KT_NODISCARD virtual auto GetCount() const -> UInt32_T { return m_Count; }

         // Temporary for OpenGL IndexBuffer
         virtual auto Bind() const -> void {}
         virtual auto Unbind() const -> void {}

         static auto CreateBuffer(const std::vector<UInt32_T>& data) -> std::shared_ptr<IndexBuffer>;
     protected:
         explicit IndexBuffer(UInt32_T id, UInt32_T count = 0) : m_Id{ id }, m_Count{ count } {}

     protected:
         UInt32_T m_Id{};
         UInt32_T m_Count{};
     };
}

#endif//KATE_ENGINE_INDEX_BUFFER_HH
