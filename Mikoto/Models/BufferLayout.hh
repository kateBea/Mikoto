//
// Created by kate on 1/4/25.
//

#ifndef BUFFERLAYOUT_HH
#define BUFFERLAYOUT_HH
#include <Common/Common.hh>
#include <initializer_list>
#include <utility>

#include <Models/BufferElement.hh>

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
            for (auto& item : m_Items) {
                item.SetOffset(offset);
                offset += item.GetSize();
                m_Stride += item.GetSize();
            }
        }

        UInt32_T m_Stride{};
        std::vector<BufferElement> m_Items{};
    };
}
#endif //BUFFERLAYOUT_HH
