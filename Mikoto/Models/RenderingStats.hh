//
// Created by kate on 1/3/25.
//

#ifndef RENDERINGSTATS_HH
#define RENDERINGSTATS_HH

namespace Mikoto {
    struct RenderingStats {
        explicit RenderingStats() = default;
        RenderingStats(const RenderingStats& other) = default;
        auto operator=(const RenderingStats& other) -> RenderingStats& = default;

        MKT_NODISCARD auto GetDrawCallsCount() const -> UInt64_T { return m_DrawCallsCount; }

        MKT_NODISCARD auto GetVertexCount() const -> UInt64_T { return m_VertexCount; }
        MKT_NODISCARD auto GetIndexCount() const -> UInt64_T { return m_IndexCount; }
        MKT_NODISCARD auto GetDrawCallCount() const -> UInt64_T { return m_DrawCallsCount; }

        MKT_NODISCARD auto GetModelsCount() const -> UInt64_T { return m_Models; }
        MKT_NODISCARD auto GetMeshesCount() const -> UInt64_T { return m_Meshes; }
        MKT_NODISCARD auto GetObjectsCount() const -> UInt64_T { return m_Objects; }
        MKT_NODISCARD auto GetSceneCamerasCount() const -> UInt64_T { return m_Cameras; }

        /**
         * Increments the total number of draw calls by the given amount
         * @param value increment value
         * */
        auto IncrementDrawCallCount(UInt64_T value) { m_DrawCallsCount += value; }

        /**
         * Increments the total number of indices by the given amount
         * @param value increment value
         * */
        auto IncrementIndexCount(UInt64_T value) { m_IndexCount += value; }

        /**
         * Increments the total number of vertices by the given amount
         * @param value increment value
         * */
        auto IncrementVertexCount(UInt64_T value) { m_VertexCount += value; }

        auto IncrementModelsCount(UInt64_T value) -> void { m_Models += value; }
        auto IncrementMeshesCount(UInt64_T value) -> void { m_Meshes += value; }
        auto IncrementObjectsCount(UInt64_T value) -> void { m_Objects += value; }
        auto IncrementSceneCamerasCount(UInt64_T value) -> void { m_Cameras += value; }

        /**
         * Sets the 0 the counters for vertices, indices and draw calls
         * */
        auto Reset() -> void {
            m_DrawCallsCount = 0;
            m_IndexCount = 0;
            m_VertexCount = 0;

            m_Models = 0;
            m_Meshes = 0;
            m_Objects = 0;
            m_Cameras = 0;
        }

    private:
        UInt64_T m_DrawCallsCount{};
        UInt64_T m_IndexCount{};
        UInt64_T m_VertexCount{};

        UInt64_T m_Models{};
        UInt64_T m_Meshes{};
        UInt64_T m_Objects{};
        UInt64_T m_Cameras{};
    };
}
#endif //RENDERINGSTATS_HH
