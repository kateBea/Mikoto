# Video Gallery

Open `Video_Gallery.mktproj` from the bundled demo projects, then press Play.

- Click the Game view. Use WASD to fly, Q to descend, E or Space to ascend, and Shift to move faster. Hold the right mouse button to look around.
- Each screen plays an MP4 clip from `Resources/Video/Demos` and emits spatial audio. The camera carries the listener.
- Path tracing samples each screen as a mesh area light. The room has no environment illumination.
- Select a screen's Video component to pause, seek, stop, or change its audio settings while playing.

Video playback currently targets progressive 8-bit H.264/4:2:0; AAC audio uses Windows Media Foundation before entering miniaudio and Steam Audio.
