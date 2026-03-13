//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_FRAME_GRAPH_STRUCTURES_HH
#define MIKOTO_FRAME_GRAPH_STRUCTURES_HH


#include <string>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Core/TimeService.hh>

#include <Assets//Texture.hh>
#include <Assets/AssetsService.hh>

#include <Material/TextureCube.hh>

#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Core/ResourceGroupBase.hh>
#include <Renderer/Core/FrameGraphBlackboard.hh>
#include <Renderer/Core/FrameGraphStructures.hh>

namespace Mikoto {
    class CommandContext;
    class GraphicsContext;

    struct ResourceNode {
        std::string Name{};
        FrameResourceState OutState{};
    };

    enum class FramePassNodeStatus {
        ACTIVE,
        SLEEPING
    };

    enum class FramePassExecutionPolicy {
        PER_FRAME,
        ON_CHANGE,
        ONCE
    };

    enum class FramePassNodeType {
        GRAPHICS,
        COMPUTE,
        TRANSFER,
        GENERIC, // For passes that not really need any kind of GPU work
    };

    struct ResourceBinding {
        std::string Name{};
        ResourceSlot Slot{ ResourceSlot::Slot_Max };
    };

    struct FramePassNode {
        std::string Name{};

        // [DEBUG]
        Time LastExecutionTime{};

        std::vector<ResourceNode> Reads{};
        std::vector<ResourceNode> Writes{};

        ankerl::unordered_dense::map<ResourceGroup, ResourceBinding> Resources{};

        FramePassNodeType Type{ FramePassNodeType::GRAPHICS };
        FramePassNodeStatus Status{ FramePassNodeStatus::ACTIVE };
        FramePassExecutionPolicy ExecutionPolicy{ FramePassExecutionPolicy::PER_FRAME };

        std::function<void( CommandContext &, FrameGraphBlackboard & )> ExecuteCallback{};

        bool IsDirty{ true };
        bool HasExecuted{ false };

        auto MarkDirty() -> void;

        MKT_NODISCARD auto IsActive() const -> bool;
        MKT_NODISCARD auto IsSleeping() const -> bool;
        MKT_NODISCARD auto IsStatus(FramePassNodeStatus status) const -> bool;
        MKT_NODISCARD auto IsExecutionPolicy(FramePassExecutionPolicy status) const -> bool;

        MKT_NODISCARD auto ShouldRun() const -> bool;
    };

    struct BufferBuilder {
        std::string Name{};
        Size SizeBytes{};

        Size ElementCount{};
        Size ElementSize{};

        BufferUsage Usage{ BufferUsage::VERTEX };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto ForElement( Size size, Size count ) -> BufferBuilder&;
        auto WithSizeBytes( Size size ) -> BufferBuilder&;
        auto WithUsage( BufferUsage usage ) -> BufferBuilder&;
        auto IsDynamic( bool value ) -> BufferBuilder&;

        auto Build( std::string_view name ) -> void;

    private:
        friend class FramePassBuilder;
        bool IsBuilt{ false };
    };
}
#endif // MIKOTO_FRAME_GRAPH_STRUCTURES_HH
