# Canvas widgets

One scene entity owns a `CanvasComponent`. The canvas owns all of its widgets;
widgets are not EnTT entities and do not need individual scene components.

In the Canvas inspector, use **Add widget** for a root widget or select a widget
and use **Add child**. Select a row to edit it. Parent changes preserve local
layout values; position may therefore change on screen. Removing a parent removes
its descendants. Move earlier/later changes painter order among siblings.

Children inherit parent transforms, visibility and enabled state. Anchors and
stretch margins use the parent's size. Children can extend outside their parent;
parent clipping and scroll views are not implemented.

Panels, outlines, images, text, lines, buttons, toggles and sliders can receive
input. Turn off **Interactable** on decorative elements to let input pass through.
Images and text use their authored bounds for input (not per-pixel alpha).
Lines and outlines test their stroke. The last drawn eligible widget receives
the pointer; disabled/hidden widgets cannot take focus or activate.

In **Play**, focus the Game window to interact. The standalone runtime uses the
same input routing. Press/release activates a widget; toggles also switch value,
and sliders retain the pointer while dragging outside their bounds. Tab/Shift+Tab
cycles focus within a canvas, Enter/Space activates, and Left/Right or Home/End
adjust a focused slider. A focused canvas owns keyboard navigation; otherwise the
foremost eligible canvas receives it. Hover, pressed and keyboard focus are shown.

Attach `CanvasControls.lua` to the canvas entity and create the four named widgets
listed at the top of the script. Give script-facing widgets unique names. IDs are
local to the canvas and saved with the hierarchy; clicks/value changes are latched
until consumed, and interaction state resets when a scene is copied or loaded.
Legacy Lua methods using one-based command indices remain available, but IDs are
preferred because hierarchy reordering does not change their target.

C++ can construct widgets with `Canvas::AddElement`, retain the returned ID, and
use `FindElement`, `SetParent`, `RemoveElement`, `ConsumeElementClick` and
`ConsumeValueChange`. Widget pointers expire when the command vector moves; store
IDs rather than pointers between edits. Reserve `Begin()` for rebuilding a canvas:
it clears all widgets. The retained hierarchy does not need `Begin()` each frame.
