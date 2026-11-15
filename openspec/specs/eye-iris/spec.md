# eye-iris Specification

## Purpose

The animated eye on the Mainframe display: frame cadence and layering, the low-poly eyeball, idle breathing, gaze with saccades and fixations, the iris as audio VU meter with its colour scales, the reaction to missing or failed audio, the beat pulse and beat dot, the mood-driven lids, sleep and the boot arrival. The message panel drawn over the eye has its own spec; the mood debug overlay belongs to the personality spec, and this spec only fixes where both sit in the layer order. How the mood values and the sleep state come about belongs to the personality. Sources: `apps/mainframe/src/eye_renderer.h`, `apps/mainframe/src/eye_renderer.cpp`, `apps/mainframe/src/iris_vu.h`, the `eyeRenderer.service(...)` call and its inputs in `apps/mainframe/src/main.cpp`, `apps/mainframe/src/voice_base_input.{h,cpp}`, `StartupSoundPreview::bootProgress()` in `apps/mainframe/src/sound_engine.cpp`, and `tests/iris_vu.cpp`.

## Requirements

### Requirement: Frame cadence and composition

The eye renderer SHALL draw at most one frame per frame interval. Each frame SHALL be drawn completely into one off-screen buffer of the display size and pushed to the display once. Outside the boot sequence the layers SHALL be, from bottom to top: eyeball, iris, pupil, mood lid, beat dot, sleep lids, then either the debug overlay or the message panel. While the debug view is on, the message panel SHALL NOT be drawn.

#### Scenario: Main loop runs faster than the frame interval
- **WHEN** the main loop calls the renderer again before the frame interval has passed since the last drawn frame
- **THEN** the renderer draws nothing and leaves the display unchanged

#### Scenario: Frame due
- **WHEN** the frame interval has passed since the last drawn frame
- **THEN** the renderer redraws the whole frame off-screen and pushes it to the display in one step

#### Scenario: Message while debugging
- **WHEN** a message panel is active while the debug view is on
- **THEN** the debug overlay is drawn over the eye and the panel is not

### Requirement: Display buffers unavailable

If the off-screen frame buffer cannot be allocated, the renderer SHALL draw nothing and try the allocation again on the next due frame. If only the eyeball layer cannot be allocated, the renderer SHALL draw the eyeball as one flat, single-coloured disc and keep all other layers.

#### Scenario: Frame buffer allocation fails
- **WHEN** the frame buffer cannot be created
- **THEN** no frame is pushed and the renderer retries on the next due frame

#### Scenario: Eyeball layer allocation fails
- **WHEN** the frame buffer exists but the eyeball layer cannot be created
- **THEN** the eye shows a flat disc instead of the faceted eyeball, with iris, pupil, lids and beat dot unchanged

### Requirement: Low-poly eyeball

The eyeball SHALL be a procedural, flat-shaded low-poly sphere on black, drawn into its own layer without textures or gradients. Each facet SHALL take one colour from a fixed light palette, chosen by the facet's sphere normal against a light from the upper right. The layer SHALL be re-rendered at most once per layer interval; gaze SHALL shift the sampled normals and the mesh rotation instead of moving the ball.

#### Scenario: Gaze to the side
- **WHEN** the gaze moves to the right and the layer interval has passed
- **THEN** the eyeball layer is re-rendered with shading and facet rotation shifted by the gaze, while the ball stays centred

#### Scenario: Between layer updates
- **WHEN** frames are drawn within one layer interval
- **THEN** they reuse the last rendered eyeball layer

### Requirement: Idle breathing

Outside the boot sequence the whole eye SHALL breathe: the eyeball, the iris radius, the pupil radius and the gaze offset of the iris SHALL share one scale factor that oscillates slowly between a lower bound and full size. Each time the normal view begins, breathing SHALL start at full size.

#### Scenario: Normal view begins after boot
- **WHEN** the first frame after the boot sequence is drawn
- **THEN** the eye is at full size and then shrinks and grows again with the breathing period

### Requirement: Gaze with saccades and fixations

The gaze SHALL alternate between fixations and short saccades to random targets within a horizontal and a vertical range. A saccade SHALL follow an S-shaped ease, and its duration SHALL grow with distance. A target closer than a minimum distance SHALL be moved to the horizontal edge of the range. The first saccade SHALL start a fixed delay after the first gaze update; the gaze SHALL NOT drift between saccades.

#### Scenario: Fixation ends
- **WHEN** the fixation time has passed
- **THEN** the gaze picks a new random target and moves there along an eased saccade

#### Scenario: Target too close
- **WHEN** a new target lies closer than the minimum distance to the current gaze
- **THEN** its horizontal coordinate is set to the left or right edge of the range, matching the sign of the random value

### Requirement: Attention drives gaze alertness

The renderer SHALL derive an alertness from the attention value: zero at or below an attention floor, rising linearly to one at attention 100, and approached smoothly over time rather than jumping. Higher alertness SHALL widen both gaze ranges, shorten saccades, and shorten the minimum and random part of each fixation.

#### Scenario: Calm eye
- **WHEN** attention stays at or below the attention floor
- **THEN** the gaze uses the narrowest ranges and the longest fixations

#### Scenario: Attention jumps to 100
- **WHEN** attention rises from 0 to 100 at once
- **THEN** alertness rises gradually over several frames, and later saccades reach the widest ranges with the shortest fixations

### Requirement: Iris and pupil geometry

The iris SHALL be a ring of triangular sectors around its centre that fill the full iris circle. The black pupil SHALL be a regular polygon drawn over the iris centre. Iris and pupil SHALL be offset from the display centre by the gaze, scaled by the breathing factor, and SHALL keep their shape while the eye looks around.

#### Scenario: Gaze offset
- **WHEN** the gaze is at an offset to the upper left
- **THEN** iris and pupil are drawn at that offset from the display centre, scaled by the current breathing factor

### Requirement: Iris VU level

The VU level SHALL come from the DC-corrected mean absolute level of each audio capture block, independent of the music gate and the tempo path. A block level at or below the noise gate SHALL map to 0, at or above the full level to 255, linearly in between. The displayed level SHALL rise to a higher value at once and fall by a fixed step per audio block, independent of the display frame rate.

#### Scenario: Quiet input
- **WHEN** a block level at or below the noise gate arrives
- **THEN** the VU target is 0

#### Scenario: Monotonic mapping
- **WHEN** block levels rise from just above the noise gate to the full level
- **THEN** the VU level never falls and reaches 255 at the full level

#### Scenario: Release after a loud block
- **WHEN** a block at full level is followed by silent blocks
- **THEN** the level falls by the release step per block and reaches 0 within the release time

### Requirement: Iris VU without usable audio

The renderer SHALL receive a VU level of 0 while audio analysis has not started, while the capture is not ready, and once no audio block has arrived for the stale time. The stale check SHALL work across a `millis()` overflow. A VU level of 0 SHALL leave all iris sectors at the silent base colour.

#### Scenario: Before the show starts
- **WHEN** the first button press has not yet requested audio analysis
- **THEN** the iris shows the silent base colour on all sectors

#### Scenario: Capture stops delivering
- **WHEN** the iris shows a high level and no audio block arrives for the stale time
- **THEN** the VU level drops to 0 without waiting for the release

#### Scenario: Capture fails to start
- **WHEN** the audio capture could not be started
- **THEN** the VU level stays 0 and the iris shows only the silent base colour

### Requirement: Iris sector activation

The VU level SHALL set how many sectors are active, proportional to the level and rounded up, so any non-zero level lights at least one. A shuffled order SHALL decide which sectors are active, scattered rather than one wedge. At a fixed drift interval the renderer SHALL swap a few active and inactive positions while the iris is partly lit. Per frame, active sectors SHALL fade up and inactive ones fade down.

#### Scenario: Partial level
- **WHEN** the VU level lights only some sectors
- **THEN** the active sectors are scattered around the iris and change places at the drift interval

#### Scenario: Full level
- **WHEN** the VU level stays at 255
- **THEN** all sectors fade up to the top of the colour scale

#### Scenario: Level 0
- **WHEN** the VU level becomes 0
- **THEN** every sector value drops to 0 in the same frame

### Requirement: Iris colour scale

Each sector SHALL be coloured by the larger of its own value and a fixed base value, so a silent iris stays visibly dark blue. The default scale SHALL run linearly from black over dark blue and light blue to magenta. Each sector SHALL then get a fixed, deterministic brightness shade of up to a quarter lighter or darker, which changes brightness but not the position on the scale.

#### Scenario: Silent iris
- **WHEN** all sector values are 0
- **THEN** every sector shows the base value's dark blue with its own fixed shade

#### Scenario: Scale anchors
- **WHEN** the colour scale is evaluated at 0, 85, 170 and 255
- **THEN** it yields black, dark blue, light blue and magenta as listed under Current values

#### Scenario: Neighbouring sectors at the same value
- **WHEN** two neighbouring sectors have the same value
- **THEN** they show lighter and darker variants of the same base colour

### Requirement: Annoyed iris palette

While annoyance is at or above the emotion threshold, the iris SHALL use a second scale running linearly from black over saturated dark red and red to reddish orange, with the same base value, sector activation and per-sector shade.

#### Scenario: Annoyance crosses the threshold
- **WHEN** annoyance rises to the emotion threshold
- **THEN** the iris switches to the red scale in the next frame, with the VU behaviour unchanged

### Requirement: Beat pulse

While the beat dot is visible, the pupil and the iris SHALL grow by a few pixels at the start of the beat and shrink back linearly to their rest size by the end of the dot window. Without a visible beat dot there SHALL be no pulse.

#### Scenario: Beat while the show runs
- **WHEN** the show clock triggers a beat
- **THEN** the pupil and iris are largest right after the beat and back at rest size when the beat dot disappears

#### Scenario: Show not running
- **WHEN** the show has not been started
- **THEN** pupil and iris stay at rest size

### Requirement: Beat dot

While the show runs, the renderer SHALL show a small dot with a black ring in the upper right corner for a short window after every show beat. It SHALL be red for a sync marker (show start or tempo adoption), otherwise yellow when the show clock's beat-in-bar value is 1 and white for other values. Before the show starts the dot SHALL NOT appear.

#### Scenario: Show start
- **WHEN** the user starts the show
- **THEN** a red dot appears for the dot window

#### Scenario: Regular beats
- **WHEN** a regular show beat leaves the beat-in-bar value at 1
- **THEN** the dot is yellow; for values 2 to 4 it is white

#### Scenario: Tempo adoption
- **WHEN** the show adopts a new tempo
- **THEN** a red dot appears for the dot window

### Requirement: Annoyance lid

While annoyance is at or above the emotion threshold, a black, low-poly upper lid SHALL cover the top of the eye, lower in the middle than at the edges, and SHALL reach further down the higher annoyance gets. It SHALL take precedence over the loneliness lid.

#### Scenario: Annoyed eye
- **WHEN** annoyance is at the emotion threshold
- **THEN** a black upper lid with a lowered middle covers the top of the eye

#### Scenario: Annoyance and loneliness both high
- **WHEN** annoyance and loneliness are both at or above the emotion threshold
- **THEN** only the annoyance lid is drawn

### Requirement: Loneliness lid and lowered gaze

While loneliness is at or above the emotion threshold and annoyance is below it, a black upper lid SHALL cover the top of the eye, higher in the middle than at the edges, with the middle rising and the edges falling as loneliness grows. Independent of annoyance, loneliness at or above the threshold SHALL lower the gaze by an amount growing with loneliness.

#### Scenario: Lonely eye
- **WHEN** loneliness rises from the emotion threshold to 100 while annoyance stays low
- **THEN** the upper lid tapers more steeply to a point in the middle and the gaze sinks further

### Requirement: Sleep lids

When the personality reports sleep, two curved black lids SHALL close from the top and bottom edges; when sleep ends they SHALL open again. Both directions SHALL take the same fixed duration with a slow-fast-slow ease. A long gap between frames SHALL NOT skip the animation. While the lids are open, nothing SHALL be drawn for them.

#### Scenario: Falling asleep
- **WHEN** the sleep state becomes true
- **THEN** the lids close over the eye within the lid duration

#### Scenario: Waking up
- **WHEN** the sleep state becomes false while the lids are closed
- **THEN** the lids open within the lid duration and the running eye becomes visible again

#### Scenario: Long frame gap
- **WHEN** the next frame comes much later than the frame interval during a lid movement
- **THEN** the lids advance by at most one capped step instead of jumping to the end

### Requirement: Closed eye

When the sleep lids are almost fully closed, the renderer SHALL clear the eye to black and draw one curved, readable closed-eye line with a bright core and a softer rim. While sleep is reported, the message panel SHALL NOT be drawn; the debug overlay still is.

#### Scenario: Asleep
- **WHEN** the lids have closed completely
- **THEN** the display shows only the closed-eye line on black, without beat dot or message panel

### Requirement: Boot arrival

While the boot progress is below 255, the renderer SHALL draw the boot sequence instead of the normal eye, without gaze, VU, lids, beat dot or sleep. In its first part a simple eye of flat concentric circles SHALL approach from small to full size with a strong ease-in, centred horizontally and hopping a few times with decaying height.

#### Scenario: Early boot
- **WHEN** the boot progress is in the arrival part
- **THEN** a small circle eye hops on black and grows slowly at first and fast just before the flash

#### Scenario: Button pressed during boot
- **WHEN** the show is started while the boot sequence runs
- **THEN** no beat dot or beat pulse is drawn until the boot sequence ends

### Requirement: Boot flash reveal

In the last part of the boot sequence the renderer SHALL draw the detailed eye, centred and with the silent iris, and blend white over it, from full white at the start of the part to none at its end. Afterwards the normal eye SHALL take over.

#### Scenario: Flash part
- **WHEN** the boot progress enters the flash part
- **THEN** the display turns white and the detailed eye fades in from under the white

### Requirement: Boot progress source

The boot progress SHALL follow the startup jingle: 0 until the jingle is due, rising linearly to 255 over the jingle duration, and 255 once the startup sound has finished. If the speaker or the jingle cannot be played, the startup sound SHALL count as finished and the boot sequence SHALL end.

#### Scenario: Normal boot
- **WHEN** the Mainframe boots with a working speaker
- **THEN** the boot sequence runs in step with the jingle and the normal eye appears when the jingle ends

#### Scenario: Speaker unavailable
- **WHEN** the speaker cannot be started at boot
- **THEN** the boot progress is 255 and the normal eye is shown from the first frame

## Current values

These are the values in the code on 2026-11-15. They are calibration and may change without a spec change.

- Frame interval: 25 ms (`kFrameIntervalMs`); eyeball layer interval: 125 ms (`kLowPolyLayerIntervalMs`); display and buffers 128×128 RGB565.
- Eyeball mesh: production build 48 triangles, 16 segments in two rings (`DECAFLASH_EYE_RENDERER_MODE` 48); benchmark builds may select 20, 40, 72 or 80. Six-step palette from `(28,43,71)` to `(255,247,227)`. Fallback disc colour `(154,171,191)`.
- Breathing: scale 0.95 to 1.0, period 5 s.
- Gaze: first saccade 900 ms after the first update; attention floor 10; alertness smoothing over 400 ms, frame gap capped at 100 ms; ranges ±(9 + 13·alertness) px horizontally and ±(5 + 7·alertness) px vertically; minimum target distance 4 px; saccade duration 60 + min(120, 5·distance)·(1 − 0.45·alertness) ms; fixation (1500 − 1150·alertness) ms plus a random part below (1600 − 1100·alertness) ms.
- Iris: 36 sectors (`IrisVu::kFacets`), radius 38 px; pupil 12-sided, radius 15 px.
- VU: one block is 256 samples (16 ms); noise gate 80, full level 2400 (`kNoiseGate`, `kFullLevel`); release 26 per block (about 160 ms from 255 to 0); stale time 250 ms (`kStaleMs`).
- Sector activation: drift every 220 ms with four swaps; fade up 18 and down 14 per frame.
- Colour: base value 52 (`kIrisBaseValue`). Default scale anchors 0 → `(0,0,0)`, 85 → `(8,34,110)`, 170 → `(40,212,255)`, 255 → `(255,30,230)`. Annoyed scale anchors 85 → `(145,6,12)`, 170 → `(255,14,17)`, 255 → `(255,65,5)`. Per-sector shade −25 % to +25 %.
- Beat dot: 140 ms (`kBeatDotFlashMs`), at x 116, y 11, radius 3 in a black ring of radius 5; red `(255,0,0)`, yellow `(255,210,0)`, white. Beat pulse: pupil up to +5 px, iris up to +2 px.
- Emotion threshold: 90 for annoyance and loneliness; loneliness lowers the gaze by 0.4 px per point above the threshold.
- Sleep lids: 360 ms per direction, step capped at 100 ms per frame; closed-eye drawing from 98 % lid progress; line colours `(176,201,255)` core and `(78,93,133)` rim.
- Boot: jingle 2400 ms, due 180 ms after the startup sound starts; flash part from progress 196 of 255 (about 550 ms); arrival scale 0.12 to 1.0 with a fourth-power ease-in, three hops of up to 25 px.

## Known gaps

Not requirements; listed so they are not mistaken for intended behaviour.

- BUG-007 in `bugs.md` (code reading only): the beat dot counts the bar correctly, but the Mainframe sends the bar's ClockSync with beat 1 on the following beat, which the dot shows white. Nodes therefore run one beat behind the dot.
- There is no idle blink. The only lid closing is the sleep animation, although `docs/project-context.md` B.2 lists blinking as implemented.
- Sector fades and drift run per drawn frame, so they slow down when frames are delayed; only the VU level itself is tied to audio blocks.
- Level calibration, visual acceptance and runtime under audio load of the iris VU are still open on the device (`docs/project-context.md`, section 0).
