# Video Gallery

Open `Video_Gallery.mktproj` from the bundled demo projects, then press Play.

- Press P in the focused Game panel to switch between bunny orbit and free-fly cameras. The canvas shows the active mode and controls.
- Orbit: WASD moves Lola, Shift runs faster, right mouse orbits, and the wheel changes camera distance.
- Free fly: WASD moves the camera, Q/E (or Space) moves vertically, right mouse looks around, and Shift moves faster. Lola stays in place.
- Each screen plays one of `Resources/Video/Demos`' MP4 clips and emits spatial audio. Lola carries the listener.
- Path tracing samples each screen as a mesh area light. The room has no environment illumination.
- Select a screen's Video component to pause, seek, stop, or change its audio settings while playing.

The bundled Lola animation is `C038_Run`. Video playback currently targets progressive 8-bit H.264/4:2:0; AAC audio uses Windows Media Foundation before entering miniaudio and Steam Audio.
