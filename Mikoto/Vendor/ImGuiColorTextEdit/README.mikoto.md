# Mikoto integration

Upstream: https://github.com/goossens/ImGuiColorTextEdit

Revision: `133614b0d5e1008527a26f46a93fdb1d751ca115` (master, vendored 2026-09-27).

`TextEditor.h` and `TextEditor.cpp` are unmodified upstream sources. Only the
editor is compiled; the optional TextDiff widget and examples are not included.
The MIT license is preserved in `LICENSE`.

Mikoto-specific language identifiers, the Retro Blue palette, and compatibility
with the editor panels live in `Mikoto/ImGui/Sources/TextEditor.cc`. Keep those
adaptations outside the vendor sources when updating this dependency. The shared
CMake integration supports both vcpkg and FetchContent dependency builds.
