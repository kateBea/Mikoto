//
// Created by kate on 1/4/25.
//

#ifndef BUFFERLAYOUT_HH
#define BUFFERLAYOUT_HH

#include <utility>
#include <algorithm>
#include <ranges>
#include <initializer_list>

#include <Common/Common.hh>
#include <Renderer/Buffer/BufferElement.hh>

namespace Mikoto {
    class BufferLayout {
    public:
        BufferLayout( std::initializer_list<BufferElement>&& items)
            : m_Items(std::forward<std::initializer_list<BufferElement>>(items))
        {
            ComputeOffsetAndStride();
        }

        MKT_NODISCARD auto GetElements() const -> const std::vector<BufferElement>& { return m_Items; }
        MKT_NODISCARD auto GetCount() const -> Size_T { return m_Items.size(); }
        MKT_NODISCARD auto GetStride() const { return m_Stride; }

        auto operator[](UInt32_T index) -> BufferElement& { return m_Items[index]; }
        auto operator[](UInt32_T index) const -> const BufferElement& { return m_Items[index]; }

        auto begin() -> std::vector<BufferElement>::iterator { return m_Items.begin(); }
        auto end() -> std::vector<BufferElement>::iterator { return m_Items.end(); }

        MKT_NODISCARD auto begin() const -> std::vector<BufferElement>::const_iterator { return m_Items.begin(); }
        MKT_NODISCARD auto end() const -> std::vector<BufferElement>::const_iterator { return m_Items.end(); }

        auto rbegin() -> std::vector<BufferElement>::reverse_iterator { return m_Items.rbegin(); }
        auto rend() -> std::vector<BufferElement>::reverse_iterator { return m_Items.rend(); }

        MKT_NODISCARD auto rbegin() const -> std::vector<BufferElement>::const_reverse_iterator { return m_Items.rbegin(); }
        MKT_NODISCARD auto rend() const -> std::vector<BufferElement>::const_reverse_iterator { return m_Items.rend(); }
    private:
        // Helpers
        auto ComputeOffsetAndStride() -> void {
            UInt32_T offset{ 0 };

            for (auto& bufferElement : m_Items) {
                bufferElement.SetOffset(offset);
                offset += bufferElement.GetSize();
                m_Stride += bufferElement.GetSize();
            }
        }

    private:
        // The size in bytes for all the elements contained
        // within this buffer layout, e.g: if this buffer layout has
        // 2 buffer elements (1 float and 1 mat4), m_Stride = Size(float) + Size(mat4)
        // where size is the size in bytes of each element, see buffer element for sizes
        UInt32_T m_Stride{};

        // The buffer elements
        std::vector<BufferElement> m_Items{};
    };
}
#endif //BUFFERLAYOUT_HH
