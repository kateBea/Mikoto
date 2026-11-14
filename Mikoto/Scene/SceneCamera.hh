//    Copyright 2026 ケイト
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

#ifndef MIKOTO_SCENE_SCENE_CAMERA_HH
#define MIKOTO_SCENE_SCENE_CAMERA_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/ReferenceCounted.hh>

#include <Platform/Window.hh>

#include <Scene/Camera.hh>

namespace mikoto::scene {

    /** Grain parameters supplied to a post-process material by its camera reference. */
    struct FilmGrainSettings {
        /** Additive RGB amplitude in [0, 1]; zero preserves the source image. */
        core::f32 mAmount{ 0.01f };
    };

    /**
     * Signed-distance shapes supported by the vignette mask pass.
     *
     * @details Values match the branches in VignetteShape_Frag.slang.
     * @see https://iquilezles.org/articles/distfunctions2d/
     */
    enum class VignetteShape : core::u8 {
        /** Circular mask whose radius is VignetteSettings::mHalfSize.x. */
        eCircle,
        /** Rectangular mask using both axes of VignetteSettings::mHalfSize. */
        eRectangle,
    };


    /**
     * Shape, placement, and compositing parameters for a camera's vignette.
     *
     * @details The mask evaluates (uv - mCenter) * 2, then optionally scales X
     * by the viewport aspect ratio. Size and feather use those coordinates.
     * SceneCamera::SetVignetteSettings sanitizes values before storing them.
     */
    struct VignetteSettings {
        /** Whether the renderer should apply this camera's vignette. */
        bool mEnabled{};
        /** Signed-distance shape used to generate the mask. */
        VignetteShape mShape{ VignetteShape::eCircle };

        /** Mask center in normalized viewport UV coordinates, in [0, 1]. */
        core::float2 mCenter{ 0.5f, 0.5f };

        /** Rectangle half-extents, or circle radius in X, in mask coordinates. */
        core::float2 mHalfSize{ 1.1f, 1.1f };

        /** Transition half-width around the shape boundary; larger values soften the edge. */
        core::f32 mFeather{ 0.25f };

        /** Composite opacity multiplier in [0, 1]; zero leaves the scene color unchanged. */
        core::f32 mStrength{ 0.4f };

        /** RGB tint blended over the scene, with each channel in [0, 1]. */
        core::float3 mColor{ 0.0f, 0.0f, 0.0f };

        /** Whether to scale mask X by render target width / height to preserve shape proportions. */
        bool mAspectCorrect{ true };
    };


    /**
     * Initial perspective lens and input window for a scene camera.
     *
     * @details Construction requires a field of view in (0, 179) degrees,
     * a positive aspect ratio, and 0 < near plane < far plane.
     * The window is borrowed and may be null when input is not needed.
     */
    struct SceneCameraDescription {
        /** Vertical field of view in degrees. */
        core::f32 mFov{ 45.0f };

        /** Viewport width divided by viewport height. */
        core::f32 mAspectRatio{ 16.0f / 9.0f };

        /** Distance to the near clipping plane in world units. */
        core::f32 mNearPlane{ 0.1f };

        /** Distance to the far clipping plane in world units. */
        core::f32 mFarPlane{ 1000.0f };

        /** Optional input window; it must outlive camera input updates. */
        platform::IWindow* mWindow{};


        /**
         * Sets the initial far clipping distance.
         *
         * @param value Distance in world units; must exceed the near plane at construction.
         * @returns This description, for chaining.
         */
        auto SetFarPlane( core::f32 value ) -> SceneCameraDescription&;


        /**
         * Sets the initial near clipping distance.
         *
         * @param value Positive distance in world units; must be below the far plane at construction.
         * @returns This description, for chaining.
         */
        auto SetNearPlane( core::f32 value ) -> SceneCameraDescription&;


        /**
         * Selects the window from which the camera will read input.
         *
         * @param window Borrowed input window, or nullptr for a camera without window input.
         * @returns This description, for chaining.
         */
        auto SetTargetWindow( platform::IWindow* window ) -> SceneCameraDescription&;


        /**
         * Sets the initial vertical field of view.
         *
         * @param value Angle in degrees; must lie strictly between 0 and 179 at construction.
         * @returns This description, for chaining.
         */
        auto SetFieldOfView( core::f32 value ) -> SceneCameraDescription&;


        /**
         * Computes the initial aspect ratio from viewport dimensions.
         *
         * @param width Viewport width, in the same units as height.
         * @param height Viewport height, in the same units as width.
         * @returns This description, for chaining.
         * @details Leaves the previous ratio unchanged if either dimension or
         * their quotient is not positive.
         */
        auto SetAspectRatio( core::f32 width, core::f32 height ) -> SceneCameraDescription&;
    };

    /**
     * Scene camera with fly/orbit navigation, damped movement, and lens effects.
     *
     * @details Mouse input controls yaw and pitch; the wheel dollies the camera without changing FOV.
     * Fly navigation moves the eye;
     * orbit navigation rotates around a pivot and supports panning and dollying.
     * Input starts disabled and requires both a target window and EnableCamera(true).
     * Programmatic view changes remain available without window input.
     * @see https://www.ryanjuckett.com/damped-springs/
     * @see https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/lookat-function/framing-lookat-function.html
     */
    class SceneCamera final : public Camera, public core::ReferenceCounted<SceneCamera> {
    public:
        /**
         * Creates a camera with the default lens, viewpoint, and fly navigation.
         *
         * @details No input window is assigned and window input starts disabled.
         */
        explicit SceneCamera();


        /**
         * Creates a perspective camera from lens and window settings.
         *
         * @param desc Initial lens parameters and optional borrowed input window.
         * @pre desc satisfies the constraints documented by SceneCameraDescription.
         * @details Uses fly navigation and leaves window input disabled.
         */
        explicit SceneCamera( const SceneCameraDescription& desc );


        /**
         * Creates a perspective camera with an explicit lens and no input window.
         *
         * @param fov Vertical field of view in degrees, strictly between 0 and 179.
         * @param aspectRatio Positive viewport width / height ratio.
         * @param nearClip Positive near clipping distance in world units.
         * @param farClip Far clipping distance in world units, greater than nearClip.
         */
        SceneCamera( core::f32 fov, core::f32 aspectRatio, core::f32 nearClip, core::f32 farClip );


        /**
         * Processes enabled input, advances navigation smoothing, and refreshes the matrices.
         *
         * @param timeStep Elapsed time in seconds; navigation advances only for positive values.
         * @param allowKeyboardMovement Whether keyboard input may move the camera or orbit pivot.
         * @details Disabling keyboard movement also clears pending translation and dolly motion,
         * while allowing mouse look to continue. Disabling camera input via EnableCamera(false)
         * still lets existing smoothing finish. Keyboard movement uses at most 0.1 seconds per update.
         * Wheel movement follows the same translation gate and uses the movement speed and smoothing.
         */
        auto Update( core::f64 timeStep, bool allowKeyboardMovement = true ) -> void;


        /**
         * Translates the eye and all navigation targets immediately. For instance, it is useful
         * for making the camera follow a moving character while keeping mouse look independent.
         * Moving only the camera position would leave its smoothing targets behind, potentially
         * pulling it back toward the old position on the next update.
         *
         * @param delta Translation in world units, applied equally to the eye and orbit pivot.
         * @details Preserves the view direction and ongoing smoothing, then refreshes the view matrix.
         */
        auto TranslateView( const core::float3& delta ) -> void;


        /**
         * Places the eye and points it at a world-space target immediately.
         *
         * @param position Eye position in world coordinates.
         * @param target World-space point to look toward and use to establish the orbit distance.
         * @details Clears navigation smoothing. Distances below
         * 0.01 world units are ignored; the stored orbit distance is capped at 1,000,000 units.
         */
        auto LookAt( const core::float3& position, const core::float3& target ) -> void;


        /**
         * Sets the eye and forward direction immediately, clearing navigation smoothing.
         *
         * @param position Eye position in world coordinates.
         * @param direction Forward vector; normalized internally.
         * @pre The squared length of direction is greater than 0.000001.
         * @details Preserves the orbit distance and places its pivot along the new view ray.
         * Accepts exact vertical directions with a deterministic orientation, and avoids
         * subtracting large world-space positions to recover a short direction.
         */
        auto SetViewPose( const core::float3& position, const core::float3& direction ) -> void;


        /**
         * Switches navigation modes without changing the displayed view.
         *
         * @param mode Fly or orbit navigation; unsupported values are ignored.
         * @details A mode change clears pending movement and spring velocities.
         */
        auto SetNavigationMode( CameraNavigationMode mode ) -> void;


        /**
         * Returns the active navigation mode.
         *
         * @returns Fly or orbit navigation.
         */
        MKT_NODISCARD auto GetNavigationMode() const -> CameraNavigationMode;


        /**
         * Changes the forward direction immediately and clears navigation smoothing.
         *
         * @param direction Desired forward vector; normalized internally.
         * @details Fly mode preserves the eye position. Orbit mode preserves the pivot
         * and distance, moving the eye around it. Near-zero vectors are ignored.
         */
        auto SetViewDirection( const core::float3& direction ) -> void;

        /** Timed editor transitions; direct setters remain immediate for scripts. */
        auto TransitionViewDirection( const core::float3& direction ) -> void;
        auto TransitionProjectionType( ProjectionType type ) -> void;


        /**
         * Returns the current orbit pivot after navigation smoothing.
         *
         * @returns Pivot in world coordinates; in fly mode this is a point along the view ray.
         */
        MKT_NODISCARD auto GetOrbitTarget() const -> core::float3;


        /**
         * Returns the current distance between the eye and orbit pivot.
         *
         * @returns Distance in world units after navigation smoothing.
         */
        MKT_NODISCARD auto GetOrbitDistance() const -> core::f32;


        /**
         * Repositions the orbit pivot along the current view ray without moving the eye.
         *
         * @param distance Distance in world units, clamped to [0.01, 1000000].
         * @details Clears navigation smoothing.
         */
        auto SetOrbitDistance( core::f32 distance ) -> void;


        /**
         * Sets keyboard translation and orbit dolly speed.
         *
         * @param value Speed in world units per second, clamped to [0.01, 5000].
         */
        auto SetMovementSpeed( core::f32 value ) -> void;


        /**
         * Sets mouse look sensitivity.
         *
         * @param value Sensitivity multiplier, clamped to [0.1, 500].
         * @details Each mouse pixel contributes 0.0005 * value radians to the target angle.
         */
        auto SetRotationSpeed( core::f32 value ) -> void;


        /**
         * Sets navigation smoothing through an inverse response-time parameter.
         *
         * @param value Positive damping rate in inverse seconds; larger values respond faster.
         * @details Converts the rate to a smooth time of 2 / value seconds through SetSmoothTime.
         * Non-positive values leave smoothing unchanged.
         * @see SetSmoothTime
         */
        auto SetDampingFactor( core::f32 value ) -> void;


        /**
         * Sets the response time of the critically damped navigation springs.
         *
         * @param seconds Response time in seconds, capped at 2; zero disables smoothing.
         * @details Negative values leave the current response time unchanged.
         */
        auto SetSmoothTime( core::f32 seconds ) -> void;


        /**
         * Selects the window used for mouse and keyboard input.
         *
         * @param window Borrowed window that must outlive input updates; nullptr is ignored.
         * @details Resets mouse sampling so the window change cannot introduce a look delta.
         */
        auto SetTargetWindow( const platform::IWindow* window ) -> void;


        /**
         * Enables mouse-controlled pitch and yaw independently.
         *
         * @param xAxis Whether vertical mouse motion may change pitch about the camera's right axis.
         * @param yAxis Whether horizontal mouse motion may change yaw about the world Y axis.
         */
        auto WantRotation( bool xAxis, bool yAxis ) -> void;


        /**
         * Returns the keyboard movement speed.
         *
         * @returns Translation and orbit dolly speed in world units per second.
         */
        MKT_NODISCARD auto GetMovementSpeed() const -> core::f32;


        /**
         * Returns the mouse look sensitivity.
         *
         * @returns Multiplier applied to the base sensitivity of 0.0005 radians per mouse pixel.
         */
        MKT_NODISCARD auto GetRotationSpeed() const -> core::f32;


        /**
         * Returns the navigation spring response time.
         *
         * @returns Time in seconds; zero means smoothing is disabled.
         */
        MKT_NODISCARD auto GetSmoothTime() const -> core::f32;


        /**
         * Reports whether mouse-controlled pitch is enabled.
         *
         * @returns True if vertical mouse motion may change pitch.
         */
        MKT_NODISCARD auto WantsXAxisRotation() const -> bool;


        /**
         * Reports whether mouse-controlled yaw is enabled.
         *
         * @returns True if horizontal mouse motion may change yaw.
         */
        MKT_NODISCARD auto WantsYAxisRotation() const -> bool;


        /**
         * Restores the scene-view navigation and lens defaults while preserving the displayed pose.
         *
         * @details Selects fly navigation, both rotation axes, the default movement response,
         * and a perspective lens with a 45-degree field of view and clipping planes at 0.1 and 5000.
         * The orthographic height is reset to 20 world units.
         */
        auto ResetSettings() -> void;


        /**
         * Restores the starting scene viewpoint and orbit distance immediately.
         *
         * @details Places the eye at (8, 6.5, 10), looks toward (0, 1, 0), and resets
         * the orbit distance to 10 world units. Clears pending navigation smoothing.
         */
        auto ResetView() -> void;


        /**
         * Enables or disables navigation input from the target window.
         *
         * @param value True to process mouse and keyboard navigation during Update.
         * @details Changing this flag resets mouse sampling. Disabling input does not
         * freeze matrices or cancel smoothing already in progress.
         */
        auto EnableCamera( bool value ) -> void;


        /**
         * Returns the vignette parameters consumed by the shape and composite passes.
         *
         * @returns Read-only reference to settings owned by this camera.
         */
        MKT_NODISCARD auto GetVignetteSettings() const -> const VignetteSettings&;

        MKT_NODISCARD auto GetFilmGrainSettings() const -> const FilmGrainSettings&;

        /** Clamps grain amplitude to [0, 1], replacing nonfinite values with the default. */
        auto SetFilmGrainSettings( const FilmGrainSettings& settings ) -> void;


        /**
         * Updates the vignette shape and composite parameters.
         *
         * @param settings Enable flag, shape, placement, size, feather, tint, and strength to copy.
         * @details Clamps center, tint, and strength to [0, 1], and size and feather to
         * [0.0001, 4]. Unsupported shapes use the circle.
         */
        auto SetVignetteSettings( const VignetteSettings& settings ) -> void;


        /**
         * Sets both rectangle half-extents, or the circle radius.
         *
         * @param value Size in mask coordinates, clamped to [0.0001, 4].
         * @details Coordinates follow VignetteSettings.
         */
        auto SetVignetteSize( core::f32 value ) -> void;


        /**
         * Selects the signed-distance shape used for the vignette mask.
         *
         * @param value Circle or rectangle; unsupported values select the circle.
         */
        auto SetVignetteShape( VignetteShape value ) -> void;


        /**
         * Sets how strongly the vignette tint is blended over the scene.
         *
         * @param value Opacity multiplier, clamped to [0, 1].
         */
        auto SetVignetteIntensity( core::f32 value ) -> void;


        /**
         * Updates depth-of-field, physical lens, and bokeh settings, then refreshes the projection.
         *
         * @param settings Parameters to copy; distances use meters, lens dimensions use
         * millimeters, blade rotation uses degrees, and blur radius uses pixels.
         * @details Clamps values to their supported ranges. Keeps the focus plane beyond the focal length.
         * With a physical
         * perspective lens enabled, focal length and sensor width determine the field of view.
         */
        auto SetDepthOfFieldSettings( const DepthOfFieldSettings& settings ) -> void;


        /**
         * Sets the aperture used to compute the depth-of-field blur.
         *
         * @param fStop Lens f-number, clamped between the configured minimum f-stop and 64.
         * @details Smaller values widen the aperture and increase defocus blur.
         */
        auto SetAperture( core::f32 fStop ) -> void;


        /**
         * Sets the focal length used by the lens and depth-of-field calculations.
         *
         * @param millimeters Focal length in millimeters, clamped to [1, 300].
         * @details Also moves the focus
         * plane beyond the lens if necessary and refreshes the projection. Changes framing
         * only when a physical perspective lens is enabled.
         * @see https://photo.stackexchange.com/questions/42767/why-is-focal-length-measured-in-millimeters
         */
        auto SetFocalLength( core::f32 millimeters ) -> void;


        /**
         * Sets the distance at which the lens is focused.
         *
         * @param distance Focus distance in world units interpreted as meters.
         * @details Clamps to at least focalLength * 0.001 + 0.001 meters and at most
         * 100000 meters.
         */
        auto SetFocusDistance( core::f32 distance ) -> void;


        /**
         * Limits the depth-of-field blur radius.
         *
         * @param value Maximum radius in pixels, clamped to [0, 64]; zero disables defocus blur.
         */
        auto SetDepthOfFieldBlur( core::f32 value ) -> void;

    private:
        /**
         * Rebuilds the view matrix from the current eye and orientation basis.
         *
         * @details Constructs orientation at the origin before translating, preserving
         * a valid basis for exact vertical views and large world-space positions.
         */
        auto UpdateViewMatrix() -> void;

        /**
         * Advances the critically damped angle and position springs.
         *
         * @param timeStep Positive elapsed time in seconds.
         * @details Orbit mode derives the eye from the smoothed pivot, orientation, and radius.
         */
        auto Interpolate( core::f64 timeStep ) -> void;

        /**
         * Accumulates mouse movement into the enabled yaw and pitch targets.
         *
         * @param timeStep Elapsed time in seconds; unused because mouse deltas are already per update.
         * @details Resets the initial sample when requested and clamps mouse-driven pitch to +/-89 degrees.
         */
        auto ProcessMouseInput( core::f64 timeStep ) -> void;

        /**
         * Dollies along the view direction using the accumulated vertical wheel offset.
         * Positive offsets move forward or reduce the orbit radius. Each wheel unit
         * moves by 0.1 seconds of keyboard travel, independent of the frame timestep.
         */
        auto ProcessMouseScroll() -> void;

        /**
         * Applies held movement keys to the active navigation targets.
         *
         * @param timeStep Elapsed movement time in seconds, capped by Update.
         * @details W/S move forward/back in fly mode and dolly in orbit mode; A/D move
         * sideways or pan the pivot. E/Space and Q move up and down along world Y.
         */
        auto ProcessKeyboardInput( core::f64 timeStep ) -> void;

    private:
        core::f32 mViewTransitionTime{ 0.5f };
        core::float4 mViewTransitionFrom{}; // Quaternion xyzw.
        core::float4 mViewTransitionTo{};
        core::float3 mViewTransitionEye{};
        core::float3 mViewTransitionPivot{};
        core::f32 mProjectionTransitionTime{ 0.5f };
        core::float4x4 mProjectionTransitionFrom{ 1.0f };

        VignetteSettings mVignette{};
        FilmGrainSettings mFilmGrain{};

        // This kind of camera responds to input from a window
        // in order to compute translations and rotations
        const platform::IWindow* mWindow{ nullptr };

        core::float3 mTargetPosition{ 0.0f, 0.0f, 0.0f };
        core::float3 mPositionVelocity{};
        core::float2 mAngleVelocity{};
        core::float2 mTargetAngles{};// yaw, pitch in radians

        CameraNavigationMode mNavigationMode{ CameraNavigationMode::eFly };
        core::float3 mOrbitTarget{};
        core::float3 mTargetOrbitTarget{};
        core::float3 mOrbitVelocity{};
        core::f32 mOrbitDistance{ 10.0f };
        core::f32 mTargetOrbitDistance{ 10.0f };
        core::f32 mOrbitDistanceVelocity{};

        // Time in seconds for the critically damped spring; zero disables smoothing.
        core::f32 mSmoothTime{ 0.13f };

        bool mWantCameraRotationX{ true };
        bool mWantCameraRotationY{ true };

        core::float2 mLastMousePosition{ 0.0f, 0.0f };

        core::f32 mRotationSpeed{ 2.f };
        core::f32 mMovementSpeed{ 2.f };

        // Avoid speedy rotations. Compensate rotation speed
        float mRotationFactor{ 0.0005f };// radians per mouse pixel, before sensitivity
        bool mResetMouseSample{ true };

        bool mAllowCameraMovementAndRotation{ false };
    };
}// namespace mikoto::scene

#endif// MIKOTO_SCENE_SCENE_CAMERA_HH
