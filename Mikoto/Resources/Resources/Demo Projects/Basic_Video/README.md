# Basic Video

This restores the original two video walls, dark rear wall, floor and four optical crystal balls from the historical Video Gallery. It is a separate World selector entry; Video Neighborhood keeps its eight original clips and town layout.

The left wall plays Dragon Ball: Sparking! ZERO, and the right wall plays the Stellar Blade rollercoaster recording. Both MP4s are bundled unchanged in Assets/Video, retaining spatialized audio. Each wall is an emissive mesh area light. Path tracing is enabled at 1920 × 1080, with the original dark environment and optical materials.

Press Play and focus Game. WASD walks relative to the camera, Shift runs, Space jumps, RMB orbits and the wheel zooms. R resets the player; I toggles foot placement. The pink Xbot, animation crossfades, virtual character controller and orbit occlusion come from the existing Animation Playground/Video Neighborhood controller. Static collision was added for the gallery floor, wall, frames and spheres.

Animated video and characters invalidate path-tracing accumulation as they move; pause playback for a converged still image. No engine code is required for this demo. The default editor navigation remains Fly.

Character model attribution is retained in Assets/Models/Character/license.txt. Video recordings were supplied by the project owner; no footage is re-encoded. No bundled file exceeds 50 MB.
