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

#ifndef MIKOTO_RENDERER_CANVAS_HH
#define MIKOTO_RENDERER_CANVAS_HH

// This guy needs to go first to
// avoid Windows sneaking in stuff
#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/AssetTexture.hh>

#include <Platform/Windows/Windows.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Text/Font.hh>
#include <Renderer/Rhi/Texture.hh>

#include <Scene/Camera.hh>

namespace mikoto::renderer {

    using CanvasElementId = core::u64;
    inline constexpr CanvasElementId kInvalidCanvasElement{};

    enum class CanvasNavigation : core::u8 {
        eNext,
        ePrevious,
        eActivate,
        eDecrease,
        eIncrease,
        eHome,
        eEnd
    };

    enum class CanvasCommandType : core::u32 {
        eQuad,
        eRect,
        eSprite,
        eText,
        eLine,
        eButton,
        eToggle,
        eSlider
    };

    enum class CanvasLayoutMode : core::u32 {
        eNone,
        eAnchored,
        eStretched
    };

    enum class SpriteScaleMode : core::u8 {
        eNone,
        eStretch,
        eFit,
        eFill
    };

    enum class SpriteBlendMode : core::u8 {
        eOpaque,
        eAlpha,
        ePremultipliedAlpha,
        eAdditive,
        eMultiply
    };

    /// Rectangle in its parent's reference pixels; positive Y points down.
    struct CanvasRect {
        core::float2 mPosition{};
        core::float2 mSize{};
    };

    /// Insets used by stretched layout, measured in parent-local pixels.
    struct CanvasEdges {
        core::f32 mLeft{};
        core::f32 mTop{};
        core::f32 mRight{};
        core::f32 mBottom{};
    };

    struct CanvasRectTransform {
        core::float2 mAnchorMin{ 0.0f, 0.0f };
        core::float2 mAnchorMax{ 0.0f, 0.0f };

        CanvasEdges mEdges{};
        core::float2 mPivot{ 0.5f, 0.5f };

        core::float2 mAnchoredPosition{};
        core::float2 mSizeDelta{};

        core::f32 mRotation{};
        core::float2 mScale{ 1.0f, 1.0f };

        CanvasLayoutMode mLayoutMode{ CanvasLayoutMode::eNone };
        /** Preserve proportions when editing image dimensions or scale in the Inspector. */
        bool mUniformResize{};

        MKT_NODISCARD auto Resolve( core::float2 parentSize ) const -> CanvasRect;
        /**
         * @brief Sets the resolved rectangle size without changing its layout mode or anchors.
         * @param size Unscaled dimensions in parent-local canvas pixels.
         * @param parentSize Resolved parent dimensions in canvas pixels.
         * @details Updates size delta for absolute/anchored layouts, or the right and bottom
         * insets for stretched layouts while retaining their top-left position.
         */
        auto SetSize( core::float2 size, core::float2 parentSize ) -> void;
    };

    struct Sprite {
        core::float2 mUv0{ 0.0f, 0.0f };
        core::float2 mUv1{ 0.0f, 0.0f };

        // Origin, relevant for rotation
        core::float2 mPivot{ 0.0f, 0.0f };
        core::float3 mColor{ 0.0f, 0.0f, 0.0f };

        renderer::rhi::TextureHandle mImageHandle{};

        SpriteScaleMode mScaleMode{};
        SpriteBlendMode mBlendMode{};

        // For roundness
        core::f32 mCornerRadius{};
    };

    struct CanvasCommand {
        // Identity belongs to the canvas, not the scene registry. Reordering
        // widgets must not redirect input or script references to another widget.
        mutable CanvasElementId mId{};
        CanvasElementId mParentId{};
        eastl::string mName{};
        CanvasCommandType mType{};

        CanvasRectTransform mTransform{};
        core::float2 mStart{};
        core::float2 mEnd{};

        renderer::FontHandle mFont{};
        // File-backed sprites retain identity; generated images use the raw overload.
        auto SetTexture( asset::AssetHandle<asset::Texture> texture ) -> void;
        auto SetTexture( renderer::rhi::TextureHandle texture ) -> void;
        MKT_NODISCARD auto GetTexture() const -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetTextureAsset() const -> const asset::AssetHandle<asset::Texture>&;

        eastl::string mText{};
        renderer::rhi::Color mColor{};

        core::f32 mThickness{};
        core::f32 mRounding{};
        core::f32 mFontSize{ 24.0f };
        core::f32 mValue{};
        bool mEnabled{ true };
        bool mVisible{ true };
        bool mInteractable{ true };
        bool mHovered{};
        bool mPressed{};
        bool mFocused{};
        bool mClicked{};
        bool mValueChanged{};

    private:
        asset::AssetHandle<asset::Texture> mTextureAsset{};
        renderer::rhi::TextureHandle mGeneratedTexture{};
    };

    struct CanvasResolvedElement {
        core::usize mCommandIndex{};
        core::u32 mDepth{};
        CanvasRect mRect{};
        core::float4x4 mLocalToCanvas{ 1.0f };
        bool mVisible{};
        bool mEnabled{};
    };

    /**
     * @brief Owns a retained widget hierarchy inside a single scene entity.
     *
     * Widget IDs are local to this canvas and survive reordering and scene
     * serialization. Parent layouts, visibility and enabled state are inherited.
     * Rendering and pointer input consume the same resolved layout in painter
     * order. Capture/focus/events are transient and are reset when copying a scene.
     */
    class Canvas {
    public:
        Canvas() = default;
        Canvas( const Canvas& other );
        Canvas( Canvas&& other ) noexcept = default;
        auto operator=( const Canvas& other ) -> Canvas&;
        auto operator=( Canvas&& other ) noexcept -> Canvas& = default;
        /// Clear authored widgets for rebuilding an immediate-mode canvas.
        auto Begin() -> void;
        auto End() -> void;
        auto SetReferenceSize( core::float2 size ) -> void;
        MKT_NODISCARD auto GetReferenceSize() const -> core::float2;
        /**
         * @brief Resolves the root canvas dimensions without stretching its reference pixels.
         * @param viewportSize Displayed image dimensions in pixels, excluding letterboxing.
         * @returns Root dimensions in reference pixels. Overlay canvases keep the reference height
         * and adjust the width to the viewport aspect ratio; world canvases retain their reference size.
         */
        MKT_NODISCARD auto GetLayoutSize( core::float2 viewportSize ) const -> core::float2;
        MKT_NODISCARD auto GetWorldScale() const -> core::f32;
        auto SetWorldScale( core::f32 value ) -> void;
        MKT_NODISCARD auto GetSortOrder() const -> core::i32;
        auto SetSortOrder( core::i32 value ) -> void;
        // Legacy indexed editing remains available. Prefer the identity-based
        // APIs for structural changes; pointers/references expire on insertion.
        auto GetEditableCommands() -> eastl::vector<CanvasCommand>&;
        /// Own a widget beneath parent (zero means canvas root). Returns zero for an invalid parent.
        MKT_NODISCARD auto AddElement( CanvasCommand command, CanvasElementId parent = kInvalidCanvasElement ) -> CanvasElementId;
        /// Remove a widget and its descendants; IDs of surviving widgets are unchanged.
        auto RemoveElement( CanvasElementId id ) -> bool;
        /// Change painter order among siblings; negative moves earlier, positive later.
        auto MoveElement( CanvasElementId id, core::i32 direction ) -> bool;
        /// Reparent without changing local layout values. Reject missing parents and cycles.
        auto SetParent( CanvasElementId id, CanvasElementId parent ) -> bool;
        MKT_NODISCARD auto FindElement( CanvasElementId id ) -> CanvasCommand*;
        MKT_NODISCARD auto FindElement( CanvasElementId id ) const -> const CanvasCommand*;
        /// Find the first named widget. Prefer unique names when scripts bind by name.
        MKT_NODISCARD auto FindElement( eastl::string_view name ) const -> CanvasElementId;
        /**
         * @brief Resolves the widget hierarchy in painter order using the supplied root dimensions.
         * @param rootSize Root dimensions in reference pixels, normally obtained from GetLayoutSize.
         * Zero dimensions use the authored reference resolution.
         * @returns Resolved elements; the view expires on the next layout resolution or structural edit.
         */
        MKT_NODISCARD auto GetResolvedElements( core::float2 rootSize = {} ) const -> eastl::span<const CanvasResolvedElement>;
        MKT_NODISCARD auto IsHierarchyValid() const -> bool;
        /**
         * @brief Updates pointer interaction against the same root layout used for rendering.
         * @param position Pointer position in canvas reference pixels.
         * @param down Whether the pointer button is held.
         * @param pressed Whether the pointer button was pressed this frame.
         * @param released Whether the pointer button was released this frame.
         * @param rootSize Root dimensions used for drawing; zero uses the authored reference resolution.
         * @returns True for a hit or active pointer capture.
         */
        auto HandlePointer( core::float2 position, bool down, bool pressed, bool released, core::float2 rootSize = {} ) -> bool;
        auto CancelPointer() -> void;
        auto ClearFocus() -> void;
        MKT_NODISCARD auto HasPointerCapture() const -> bool;
        MKT_NODISCARD auto HasFocus() const -> bool;
        /**
         * @brief Indicates whether keyboard navigation should display a focus outline.
         * @returns True when a widget has keyboard navigation focus; mouse clicks retain logical
         * focus for subsequent key input without displaying the outline.
         */
        MKT_NODISCARD auto HasNavigationFocus() const -> bool;
        auto HandleNavigation( CanvasNavigation action ) -> bool;
        auto ConsumeClick( core::usize command ) -> bool;
        /// Read and clear the latched activation event (pointer release or keyboard activation).
        auto ConsumeElementClick( CanvasElementId id ) -> bool;
        /// Read and clear a value-change event; scripts need not poll floating-point values.
        auto ConsumeValueChange( CanvasElementId id ) -> bool;
        /// Clamp values to [0,1] and latch a change only when the value differs.
        auto SetValue( CanvasElementId id, core::f32 value ) -> void;

        auto SetOverlay( bool isOverlay ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto DrawQuad( const CanvasRectTransform& rect, const renderer::rhi::Color& color ) -> void;
        auto DrawRect( const CanvasRectTransform& rect, const renderer::rhi::Color& color, core::f32 rounding = 0.0f, core::f32 thickness = 1.0f ) -> void;

        auto DrawSprite( renderer::rhi::TextureHandle texture, const CanvasRectTransform& rect, const renderer::rhi::Color& color = renderer::rhi::Color{ 1, 1, 1, 1 } ) -> void;
        auto DrawSprite( asset::AssetHandle<asset::Texture> texture, const CanvasRectTransform& rect, const renderer::rhi::Color& color = renderer::rhi::Color{ 1, 1, 1, 1 } ) -> void;

        auto DrawText( renderer::FontHandle font, eastl::string_view text, core::float2 position, const renderer::rhi::Color& color ) -> void;

        auto DrawLine( core::float2 start, core::float2 end, core::f32 thickness, const renderer::rhi::Color& color ) -> void;
        // Retained controls: the returned zero-based index is used by ConsumeClick.
        auto AddButton( const CanvasRectTransform& rect, eastl::string_view label, const rhi::Color& color ) -> core::usize;
        auto AddToggle( const CanvasRectTransform& rect, eastl::string_view label, bool checked, const rhi::Color& color ) -> core::usize;
        auto AddSlider( const CanvasRectTransform& rect, float value, const rhi::Color& color ) -> core::usize;

        MKT_NODISCARD auto IsOverlay() const -> bool;
        MKT_NODISCARD auto GetCamera() const -> const scene::Camera*;

        MKT_NODISCARD auto GetCommands() const -> eastl::span<const CanvasCommand>;

    private:
        auto RebuildIndex() const -> void;
        auto SetFocus( CanvasElementId id, bool navigation = false ) -> void;
        auto Activate( CanvasCommand& command ) -> void;
        MKT_NODISCARD auto HitTest( const CanvasResolvedElement& element, core::float2 position ) const -> bool;
        MKT_NODISCARD auto ToLocal( const CanvasResolvedElement& element, core::float2 position ) const -> core::float2;

        struct HierarchyLink {
            core::usize mParent{ ~core::usize{} };
            core::usize mFirstChild{ ~core::usize{} };
            core::usize mNextSibling{ ~core::usize{} };
            core::usize mLastChild{ ~core::usize{} };
            core::usize mResolvedIndex{ ~core::usize{} };
        };
        const scene::Camera* mCamera{ nullptr };
        eastl::vector<CanvasCommand> mCommands{};
        mutable ankerl::unordered_dense::map<CanvasElementId, core::usize> mElementIndices{};
        mutable eastl::vector<HierarchyLink> mHierarchy{};
        mutable eastl::vector<CanvasResolvedElement> mResolvedElements{};
        mutable eastl::vector<core::usize> mTraversal{};
        mutable CanvasElementId mNextElementId{ 1 };
        mutable bool mIndexDirty{ true };

        // This is only here because we can visualize the canvas in 3D
        // or have it as an overlay
        bool mIsOverlay{ true };
        core::float2 mReferenceSize{ 1920, 1080 };
        core::f32 mWorldScale{ 0.001f };
        core::i32 mSortOrder{};
        CanvasElementId mPressedElement{};
        CanvasElementId mFocusedElement{};
        bool mNavigationFocus{};
    };

}// namespace mikoto::renderer

#endif//MIKOTO_RENDERER_CANVAS_HH

