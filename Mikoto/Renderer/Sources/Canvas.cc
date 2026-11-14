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

#include <cmath>
#include <limits>

#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Core/Exception.hh>

#include <Renderer/Interface/Canvas.hh>

namespace mikoto::renderer {
    using namespace core;

    static constexpr core::usize kMissing{ ~core::usize{} };
    static auto ClearState( CanvasCommand& command ) -> void {
        command.mClicked = false;
        command.mValueChanged = false;
        command.mHovered = false;
        command.mPressed = false;
        command.mFocused = false;
    }


    Canvas::Canvas( const Canvas& other ) {
        *this = other;
    }

    auto Canvas::operator=( const Canvas& other ) -> Canvas& {
        if ( this == &other ) {
            return *this;
        }

        mCommands = other.mCommands;
        mIsOverlay = other.mIsOverlay;
        mReferenceSize = other.mReferenceSize;
        mWorldScale = other.mWorldScale;
        mSortOrder = other.mSortOrder;
        mNextElementId = other.mNextElementId;

        // A runtime scene copy owns new interaction state and must not keep a
        // camera pointer, pending click or capture from its source scene.

        mCamera = nullptr;
        mPressedElement = mFocusedElement = 0;
        mNavigationFocus = false;
        mIndexDirty = true;
        mResolvedElements.clear();

        for ( auto& command: mCommands ) {
            ClearState( command );
        }

        return *this;
    }

    auto Canvas::RebuildIndex() const -> void {
        if ( !mIndexDirty ) {
            return;
        }

        mElementIndices.clear();
        // Old scenes and callers using the legacy editable list have no IDs.
        // Assign metadata once; ordinary frame updates reuse the lookup storage.
        for ( const auto& command: mCommands ) {
            if ( command.mId == std::numeric_limits<CanvasElementId>::max() ) {
                MKT_THROW_RUNTIME_ERROR( "Canvas element ID exhausted." );
            }

            mNextElementId = eastl::max( mNextElementId, command.mId + 1 );
        }

        for ( core::usize i{}; i < mCommands.size(); ++i ) {
            auto& id{ mCommands[i].mId };

            if ( !id ) {
                if ( mNextElementId == std::numeric_limits<CanvasElementId>::max() ) {
                    MKT_THROW_RUNTIME_ERROR( "Canvas element ID exhausted." );
                }

                id = mNextElementId++;
            }

            if ( !mElementIndices.emplace( id, i ).second ) {
                MKT_THROW_RUNTIME_ERROR( "Duplicate Canvas element ID." );
            }
        }

        mIndexDirty = false;
    }

    auto Canvas::GetEditableCommands() -> eastl::vector<CanvasCommand>& {
        mIndexDirty = true;
        return mCommands;
    }

    auto Canvas::GetCommands() const -> eastl::span<const CanvasCommand> {
        RebuildIndex();
        return mCommands;
    }

    auto Canvas::FindElement( CanvasElementId id ) const -> const CanvasCommand* {
        RebuildIndex();
        const auto found{ mElementIndices.find( id ) };
        return found == mElementIndices.end() ? nullptr : &mCommands[found->second];
    }

    auto Canvas::FindElement( CanvasElementId id ) -> CanvasCommand* {
        return const_cast<CanvasCommand*>( static_cast<const Canvas*>( this )->FindElement( id ) );
    }

    auto Canvas::FindElement( eastl::string_view name ) const -> CanvasElementId {
        RebuildIndex();

        for ( const auto& command: mCommands ) {
            if ( command.mName == name ) {
                return command.mId;
            }
        }

        return 0;
    }

    auto Canvas::AddElement( CanvasCommand command, CanvasElementId parent ) -> CanvasElementId {
        RebuildIndex();

        if ( parent && !FindElement( parent ) ) {
            return 0;
        }

        if ( mNextElementId == std::numeric_limits<CanvasElementId>::max() ) {
            return 0;
        }

        command.mId = mNextElementId++;
        command.mParentId = parent;

        ClearState( command );

        const auto id{ command.mId };
        mCommands.push_back( eastl::move( command ) );
        mElementIndices.emplace( id, mCommands.size() - 1 );
        return id;
    }

    auto Canvas::SetParent( CanvasElementId id, CanvasElementId parent ) -> bool {
        auto* command{ FindElement( id ) };

        if ( !command || ( parent && !FindElement( parent ) ) ) {
            return false;
        }

        // Reject cycles before touching the hierarchy; input and drawing must
        // always terminate even when an editor reparents a large subtree.
        auto ancestor{ parent };

        for ( core::usize depth{}; ancestor; ++depth ) {
            if ( ancestor == id || depth >= mCommands.size() ) {
                return false;
            }

            const auto* value{ FindElement( ancestor ) };

            if ( !value ) {
                return false;
            }

            ancestor = value->mParentId;
        }

        command->mParentId = parent;

        CancelPointer();

        return true;
    }

    auto Canvas::MoveElement( CanvasElementId id, core::i32 direction ) -> bool {
        RebuildIndex();

        const auto found{ mElementIndices.find( id ) };

        if ( found == mElementIndices.end() || !direction ) {
            return false;
        }

        const auto index{ found->second };
        const auto parent{ mCommands[index].mParentId };

        for ( core::i64 next{ static_cast<core::i64>( index ) + ( direction < 0 ? -1 : 1 ) };
            next >= 0 && next < static_cast<core::i64>( mCommands.size() ); next += direction < 0 ? -1 : 1 ) {
            if ( mCommands[next].mParentId == parent ) {
                eastl::swap( mCommands[index], mCommands[next] );
                mIndexDirty = true;
                return true;
            }
        }

        return false;
    }

    auto Canvas::RemoveElement( CanvasElementId id ) -> bool {
        if ( !FindElement( id ) ) {
            return false;
        }

        const auto elements{ GetResolvedElements() };
        eastl::vector<CanvasElementId> removed;
        bool removing{};
        core::u32 depth{};

        for ( const auto& element: elements ) {
            const auto candidate{ mCommands[element.mCommandIndex].mId };

            if ( candidate == id ) {
                removing = true;
                depth = element.mDepth;
            }
            else if ( removing && element.mDepth <= depth ) {
                break;
            }

            if ( removing ) {
                removed.push_back( candidate );
            }
        }

        // Mark first, compact once: deleting a parent removes its whole subtree
        // without repeatedly shifting the retained command buffer.
        for ( const auto key: removed ) {
            mCommands[mElementIndices.at( key )].mId = 0;
        }

        eastl::erase_if( mCommands, []( const CanvasCommand& command ) {
            return !command.mId;
        } );

        mIndexDirty = true;

        CancelPointer();

        if ( !FindElement( mFocusedElement ) ) {
            ClearFocus();
        }

        return true;
    }

    auto CanvasRectTransform::Resolve( core::float2 parentSize ) const -> CanvasRect {
        const core::float2 anchorMin{ parentSize * mAnchorMin };
        const core::float2 anchorMax{ parentSize * mAnchorMax };

        if ( mLayoutMode == CanvasLayoutMode::eNone ) {
            return { mAnchoredPosition, mSizeDelta };
        }

        if ( mLayoutMode == CanvasLayoutMode::eStretched ) {
            const core::float2 minimum{ anchorMin.x + mEdges.mLeft, anchorMin.y + mEdges.mTop };
            const core::float2 maximum{ anchorMax.x - mEdges.mRight, anchorMax.y - mEdges.mBottom };
            return { minimum, maximum - minimum };
        }

        const auto size{ anchorMax - anchorMin + mSizeDelta };
        return { glm::mix( anchorMin, anchorMax, mPivot ) + mAnchoredPosition - size * mPivot, size };
    }

    auto CanvasRectTransform::SetSize( core::float2 size, core::float2 parentSize ) -> void {
        const auto anchorSpan{ parentSize * ( mAnchorMax - mAnchorMin ) };

        if ( mLayoutMode == CanvasLayoutMode::eStretched ) {
            mEdges.mRight = anchorSpan.x - mEdges.mLeft - size.x;
            mEdges.mBottom = anchorSpan.y - mEdges.mTop - size.y;
        }
        else {
            mSizeDelta = mLayoutMode == CanvasLayoutMode::eAnchored ? size - anchorSpan : size;
        }
    }

    auto Canvas::GetResolvedElements( core::float2 rootSize ) const -> eastl::span<const CanvasResolvedElement> {
        if ( rootSize.x <= 0 || rootSize.y <= 0 ) {
            rootSize = mReferenceSize;
        }

        RebuildIndex();
        mResolvedElements.clear();
        mHierarchy.assign( mCommands.size(), HierarchyLink{} );
        mTraversal.clear();
        core::usize firstRoot{ kMissing };
        core::usize lastRoot{ kMissing };

        for ( core::usize i{}; i < mCommands.size(); ++i ) {
            const auto parentId{ mCommands[i].mParentId };
            const auto parent{ mElementIndices.find( parentId ) };

            if ( parentId && parent == mElementIndices.end() ) {
                continue;
            }

            const core::usize parentIndex{ parentId ? parent->second : kMissing };
            mHierarchy[i].mParent = parentIndex;

            auto& first{ parentIndex == kMissing ? firstRoot : mHierarchy[parentIndex].mFirstChild };
            auto& last{ parentIndex == kMissing ? lastRoot : mHierarchy[parentIndex].mLastChild };

            if ( last != kMissing ) {
                mHierarchy[last].mNextSibling = i;
            }
            else {
                first = i;
            }

            last = i;
        }

        if ( firstRoot != kMissing ) {
            mTraversal.push_back( firstRoot );
        }

        // Iterative preorder avoids stack overflow for deeply nested UI. Each
        // widget is visited once, in the same painter order used for hit testing.
        while ( !mTraversal.empty() ) {
            const auto index{ mTraversal.back() };
            mTraversal.pop_back();
            auto& link{ mHierarchy[index] };
            const auto& command{ mCommands[index] };
            const CanvasResolvedElement* parent{ link.mParent == kMissing ? nullptr : &mResolvedElements[mHierarchy[link.mParent].mResolvedIndex] };
            const auto rect{ command.mTransform.Resolve( parent ? parent->mRect.mSize : rootSize ) };
            const auto pivot{ rect.mSize * command.mTransform.mPivot };
            CanvasResolvedElement element{};
            element.mCommandIndex = index;
            element.mDepth = parent ? parent->mDepth + 1 : 0;
            element.mRect = { {}, rect.mSize };
            element.mLocalToCanvas = ( parent ? parent->mLocalToCanvas : core::float4x4{ 1 } ) *
                                     glm::translate( core::float4x4{ 1 }, core::float3{ rect.mPosition + pivot, 0 } ) *
                                     glm::rotate( core::float4x4{ 1 }, glm::radians( command.mTransform.mRotation ), core::float3{ 0, 0, 1 } ) *
                                     glm::scale( core::float4x4{ 1 }, core::float3{ command.mTransform.mScale, 1 } ) *
                                     glm::translate( core::float4x4{ 1 }, core::float3{ -pivot, 0 } );
            element.mVisible = command.mVisible && ( !parent || parent->mVisible );
            element.mEnabled = command.mEnabled && ( !parent || parent->mEnabled );
            link.mResolvedIndex = mResolvedElements.size();
            mResolvedElements.push_back( element );

            if ( link.mNextSibling != kMissing ) {
                mTraversal.push_back( link.mNextSibling );
            }

            if ( link.mFirstChild != kMissing ) {
                mTraversal.push_back( link.mFirstChild );
            }
        }

        return mResolvedElements;
    }

    auto Canvas::IsHierarchyValid() const -> bool {
        return GetResolvedElements().size() == mCommands.size();
    }

    auto Canvas::ToLocal( const CanvasResolvedElement& element, core::float2 position ) const -> core::float2 {
        const core::f32 determinant{ glm::determinant( element.mLocalToCanvas ) };

        if ( glm::abs( determinant ) < 1e-10f ) {
            return core::float2{ std::numeric_limits<core::f32>::infinity() };
        }

        return core::float2{ glm::inverse( element.mLocalToCanvas ) * core::float4{ position, 0, 1 } };
    }

    auto Canvas::HitTest( const CanvasResolvedElement& element, core::float2 position ) const -> bool {
        const auto& command{ mCommands[element.mCommandIndex] };

        if ( !element.mVisible || !element.mEnabled || !command.mInteractable ) {
            return false;
        }

        const auto local{ ToLocal( element, position ) };

        if ( command.mType == CanvasCommandType::eLine ) {
            const auto segment{ command.mEnd - command.mStart };
            const core::f32 lengthSquared{ glm::dot( segment, segment ) };

            if ( lengthSquared <= 0 || command.mThickness <= 0 ) {
                return false;
            }

            const core::f32 t{ glm::dot( local - command.mStart, segment ) / lengthSquared };

            if ( t < 0 || t > 1 ) {
                return false;
            }

            return glm::length( local - command.mStart - segment * t ) <= command.mThickness * 0.5f;
        }

        const auto size{ element.mRect.mSize };

        if ( size.x <= 0 || size.y <= 0 || local.x < 0 || local.y < 0 || local.x >= size.x || local.y >= size.y ) {
            return false;
        }

        const core::f32 radius{ command.mType == CanvasCommandType::eText || command.mType == CanvasCommandType::eSlider ? 0.0f : glm::clamp( command.mRounding, 0.0f, glm::min( size.x, size.y ) * 0.5f ) };
        const auto q{ glm::abs( local - size * 0.5f ) - size * 0.5f + radius };
        const core::f32 distance{ glm::length( glm::max( q, core::float2{ 0 } ) ) + glm::min( glm::max( q.x, q.y ), 0.0f ) - radius };
        return distance <= 0 && ( command.mType != CanvasCommandType::eRect || distance >= -command.mThickness );
    }

    auto Canvas::SetFocus( CanvasElementId id, bool navigation ) -> void {
        mFocusedElement = id;
        mNavigationFocus = id != 0 && navigation;

        for ( auto& command: mCommands ) {
            command.mFocused = command.mId == id && id != 0;
        }
    }

    auto Canvas::ClearFocus() -> void {
        SetFocus( 0 );
    }

    auto Canvas::CancelPointer() -> void {
        mPressedElement = 0;

        for ( auto& command: mCommands ) {
            command.mHovered = command.mPressed = false;
        }
    }

    auto Canvas::HasPointerCapture() const -> bool {
        return mPressedElement != 0;
    }

    auto Canvas::HasFocus() const -> bool {
        return mFocusedElement != 0;
    }

    auto Canvas::HasNavigationFocus() const -> bool {
        return mNavigationFocus && HasFocus();
    }

    auto Canvas::SetValue( CanvasElementId id, core::f32 value ) -> void {
        auto* command{ FindElement( id ) };

        if ( !command ) {
            return;
        }

        value = glm::clamp( value, 0.0f, 1.0f );

        if ( value != command->mValue ) {
            command->mValue = value;
            command->mValueChanged = true;
        }
    }

    auto Canvas::Activate( CanvasCommand& command ) -> void {
        command.mClicked = true;

        if ( command.mType == CanvasCommandType::eToggle ) {
            SetValue( command.mId, command.mValue > 0.5f ? 0.0f : 1.0f );
        }
    }

    auto Canvas::ConsumeClick( core::usize index ) -> bool {
        if ( index >= mCommands.size() ) {
            return false;
        }

        const bool clicked{ mCommands[index].mClicked };
        mCommands[index].mClicked = false;
        return clicked;
    }

    auto Canvas::ConsumeElementClick( CanvasElementId id ) -> bool {
        auto* command{ FindElement( id ) };

        if ( !command ) {
            return false;
        }

        const bool clicked{ command->mClicked };
        command->mClicked = false;
        return clicked;
    }

    auto Canvas::ConsumeValueChange( CanvasElementId id ) -> bool {
        auto* command{ FindElement( id ) };

        if ( !command ) {
            return false;
        }

        const bool changed{ command->mValueChanged };
        command->mValueChanged = false;
        return changed;
    }

    auto Canvas::HandlePointer( core::float2 position, bool down, bool pressed, bool released, core::float2 rootSize ) -> bool {
        const auto elements{ GetResolvedElements( rootSize ) };
        const CanvasResolvedElement* hovered{};
        const CanvasResolvedElement* captured{};
        bool focusValid{};

        for ( auto& command: mCommands ) {
            command.mHovered = command.mPressed = false;
        }

        for ( const auto& element: elements ) {
            auto& command{ mCommands[element.mCommandIndex] };

            if ( HitTest( element, position ) ) {
                hovered = &element;
            }

            if ( element.mVisible && element.mEnabled && command.mInteractable ) {
                if ( command.mId == mPressedElement ) {
                    captured = &element;
                }

                focusValid |= command.mId == mFocusedElement;
            }
        }

        if ( !focusValid ) {
            ClearFocus();
        }

        if ( hovered ) {
            mCommands[hovered->mCommandIndex].mHovered = true;
        }

        if ( pressed ) {
            captured = hovered;
            mPressedElement = hovered ? mCommands[hovered->mCommandIndex].mId : 0;
            SetFocus( mPressedElement );
        }

        if ( !captured ) {
            mPressedElement = 0;
            return hovered != nullptr;
        }

        auto& command{ mCommands[captured->mCommandIndex] };
        command.mPressed = down;
        // Capture owns the drag until release, even outside the widget/canvas.
        // A different control under the release position never receives the click.
        if ( command.mType == CanvasCommandType::eSlider && ( down || released ) ) {
            SetValue( command.mId, ToLocal( *captured, position ).x / glm::max( 1.0f, captured->mRect.mSize.x ) );
        }

        if ( released ) {
            if ( HitTest( *captured, position ) ) {
                Activate( command );
            }

            mPressedElement = 0;
            command.mPressed = false;
        }
        else if ( !down ) {
            mPressedElement = 0;
            command.mPressed = false;
        }

        return true;
    }

    auto Canvas::HandleNavigation( CanvasNavigation action ) -> bool {
        const auto elements{ GetResolvedElements() };
        eastl::vector<CanvasElementId> eligible;

        for ( const auto& element: elements ) {
            const auto& command{ mCommands[element.mCommandIndex] };

            if ( element.mVisible && element.mEnabled && command.mInteractable ) {
                eligible.push_back( command.mId );
            }
        }

        if ( eligible.empty() ) {
            ClearFocus();
            return false;
        }

        const auto found{ eastl::find( eligible.begin(), eligible.end(), mFocusedElement ) };

        if ( action == CanvasNavigation::eNext || action == CanvasNavigation::ePrevious ) {
            core::i64 index{ found == eligible.end() ? ( action == CanvasNavigation::eNext ? -1 : 0 ) : found - eligible.begin() };
            index = ( index + ( action == CanvasNavigation::eNext ? 1 : -1 ) + eligible.size() ) % eligible.size();
            SetFocus( eligible[index], true );
            return true;
        }

        if ( found == eligible.end() ) {
            ClearFocus();
            return false;
        }

        auto& command{ *FindElement( mFocusedElement ) };

        if ( action == CanvasNavigation::eActivate ) {
            SetFocus( command.mId, true );
            Activate( command );
            return true;
        }

        if ( command.mType != CanvasCommandType::eSlider ) {
            return false;
        }

        const core::f32 value{ action == CanvasNavigation::eHome ? 0.0f : action == CanvasNavigation::eEnd ? 1.0f
                                                                                                           : command.mValue + ( action == CanvasNavigation::eDecrease ? -0.05f : 0.05f ) };

        SetFocus( command.mId, true );
        SetValue( command.mId, value );

        return true;
    }

    auto Canvas::Begin() -> void {
        mCommands.clear();
        mIndexDirty = true;
        CancelPointer();
        ClearFocus();
    }

    auto Canvas::End() -> void {
    }

    auto Canvas::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto Canvas::GetCamera() const -> const scene::Camera* {
        return mCamera;
    }

    auto Canvas::SetOverlay( bool overlay ) -> void {
        mIsOverlay = overlay;
    }

    auto Canvas::IsOverlay() const -> bool {
        return mIsOverlay;
    }

    auto Canvas::SetReferenceSize( core::float2 size ) -> void {
        mReferenceSize = glm::max( size, core::float2{ 1 } );
    }

    auto Canvas::GetReferenceSize() const -> core::float2 {
        return mReferenceSize;
    }

    auto Canvas::GetLayoutSize( core::float2 viewportSize ) const -> core::float2 {
        if ( !mIsOverlay ) {
            return mReferenceSize;
        }

        // Height controls UI scale. Narrowing a viewport changes the available
        // layout width without shrinking every widget and its text.
        viewportSize = glm::max( viewportSize, core::float2{ 1 } );
        const auto scale{ viewportSize.y / mReferenceSize.y };
        return viewportSize / scale;
    }

    auto Canvas::SetWorldScale( core::f32 value ) -> void {
        mWorldScale = glm::max( value, 0.000001f );
    }

    auto Canvas::GetWorldScale() const -> core::f32 {
        return mWorldScale;
    }

    auto Canvas::SetSortOrder( core::i32 value ) -> void {
        mSortOrder = value;
    }

    auto Canvas::GetSortOrder() const -> core::i32 {
        return mSortOrder;
    }

    auto Canvas::DrawQuad( const CanvasRectTransform& rect, const rhi::Color& color ) -> void {
        CanvasCommand command{};
        command.mType = CanvasCommandType::eQuad;
        command.mTransform = rect;
        command.mColor = color;
        static_cast<void>( AddElement( eastl::move( command ) ) );
    }

    auto Canvas::DrawRect( const CanvasRectTransform& rect, const rhi::Color& color, core::f32 rounding, core::f32 thickness ) -> void {
        DrawQuad( rect, color );
        auto& command{ mCommands.back() };
        command.mType = CanvasCommandType::eRect;
        command.mRounding = glm::max( 0.0f, rounding );
        command.mThickness = glm::max( 0.0f, thickness );
    }

    auto CanvasCommand::SetTexture( asset::AssetHandle<asset::Texture> texture ) -> void {
        mTextureAsset = eastl::move( texture );
        mGeneratedTexture.Reset();
    }

    auto CanvasCommand::SetTexture( rhi::TextureHandle texture ) -> void {
        mTextureAsset = {};
        mGeneratedTexture = eastl::move( texture );
    }

    auto CanvasCommand::GetTexture() const -> rhi::TextureHandle {
        if ( mTextureAsset ) {
            const auto resource{ mTextureAsset.GetIfReady() };
            return resource ? resource->GetResource() : rhi::TextureHandle{};
        }

        return mGeneratedTexture;
    }

    auto CanvasCommand::GetTextureAsset() const -> const asset::AssetHandle<asset::Texture>& {
        return mTextureAsset;
    }

    auto Canvas::DrawSprite( asset::AssetHandle<asset::Texture> texture, const CanvasRectTransform& rect, const rhi::Color& color ) -> void {
        DrawQuad( rect, color );
        mCommands.back().mType = CanvasCommandType::eSprite;
        mCommands.back().SetTexture( eastl::move( texture ) );
    }

    auto Canvas::DrawSprite( rhi::TextureHandle texture, const CanvasRectTransform& rect, const rhi::Color& color ) -> void {
        DrawQuad( rect, color );
        mCommands.back().mType = CanvasCommandType::eSprite;
        mCommands.back().SetTexture( texture );
    }

    auto Canvas::DrawText( FontHandle font, eastl::string_view text, core::float2 position, const rhi::Color& color ) -> void {
        CanvasCommand command{};
        command.mType = CanvasCommandType::eText;
        command.mFont = font;
        command.mText = text;
        command.mTransform.mAnchoredPosition = position;
        command.mTransform.mSizeDelta = { 240, 40 };
        command.mColor = color;
        static_cast<void>( AddElement( eastl::move( command ) ) );
    }

    auto Canvas::DrawLine( core::float2 start, core::float2 end, core::f32 thickness, const rhi::Color& color ) -> void {
        CanvasCommand command{};
        command.mType = CanvasCommandType::eLine;
        const auto origin{ glm::min( start, end ) };
        command.mTransform.mAnchoredPosition = origin;
        command.mTransform.mSizeDelta = glm::max( glm::abs( end - start ), core::float2{ thickness } );
        command.mStart = start - origin;
        command.mEnd = end - origin;
        command.mThickness = glm::max( 0.0f, thickness );
        command.mColor = color;
        static_cast<void>( AddElement( eastl::move( command ) ) );
    }

    auto Canvas::AddButton( const CanvasRectTransform& rect, eastl::string_view label, const rhi::Color& color ) -> core::usize {
        DrawQuad( rect, color );
        auto& command{ mCommands.back() };
        command.mType = CanvasCommandType::eButton;
        command.mText = label;
        command.mRounding = 4;
        return mCommands.size() - 1;
    }

    auto Canvas::AddToggle( const CanvasRectTransform& rect, eastl::string_view label, bool checked, const rhi::Color& color ) -> core::usize {
        const auto index{ AddButton( rect, label, color ) };
        mCommands[index].mType = CanvasCommandType::eToggle;
        mCommands[index].mValue = checked ? 1.0f : 0.0f;
        return index;
    }

    auto Canvas::AddSlider( const CanvasRectTransform& rect, core::f32 value, const rhi::Color& color ) -> core::usize {
        const auto index{ AddButton( rect, "", color ) };
        mCommands[index].mType = CanvasCommandType::eSlider;
        mCommands[index].mValue = glm::clamp( value, 0.0f, 1.0f );
        return index;
    }
}// namespace mikoto::renderer
