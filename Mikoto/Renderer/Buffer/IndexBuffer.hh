/**
 * IndexBuffer.hh
 * Created by kate on 6/5/23.
 * */

#ifndef MIKOTO_INDEX_BUFFER_HH
#define MIKOTO_INDEX_BUFFER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include "Common/Common.hh"
#include "STL/Utility/Types.hh"

namespace Mikoto {
    class IndexBuffer {
    public:
        /**
          * Default destructor. Performs cleanup
          * */
        virtual ~IndexBuffer() = default;

        /**
          * Returns the count of indices in this index buffer
          * @returns count of indices
          * */
        MKT_NODISCARD auto GetCount() const -> UInt64_T { return m_Count; }

        /**
          * Returns the size in bytes of this index buffer
          * @returns size in bytes
          * */
        MKT_NODISCARD auto GetSize() const -> UInt64_T { return m_Size; }


        /**
         * Creates and initializes and index buffer object valid for the current
         * graphics API backend using from the data passed in
         * @param data index buffer indices
         * @returns a pointer to the newly created index buffer
         * */
        MKT_NODISCARD static auto Create(const std::vector<UInt32_T>& data) -> std::shared_ptr<IndexBuffer>;

    protected:
        /**
         * Constructs a new index buffer
         * @param count count of indices
         * @param size size in bytes of the buffer
         * */
        explicit IndexBuffer(UInt64_T count = 0U, UInt64_T size = 0U) : m_Count{ count }, m_Size{ size } {}

    protected:
        UInt64_T m_Count{};
        UInt64_T m_Size{};
    };
}

#endif // MIKOTO_INDEX_BUFFER_HH
