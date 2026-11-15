# node-output Specification

## Purpose

How a light Node turns the shared scenes into light: the scene table shared with the Mainframe, the roles per Node kind and how a Node derives its role-specific command from a scene index, the flashlight and RGB strip patterns and when they fire on the beat, how a `NodeVisualState` directive overrides the scene output, and how a `NodeText` overlay is rendered as beat-bound Morse light. Packets, receive validation, the clock, run modes and the reaction to hello are specified in `protocol`; Node configuration, the button and serial commands belong to `node-device`. Sources: `shared/include/scene_programs.h`, `shared/include/decaflash_types.h`, `apps/node/src/node_programs.h`, `apps/node/src/node_output.{h,cpp}`, `apps/node/src/flashlight_renderer.{h,cpp}`, `apps/node/src/rgb_strip_renderer.{h,cpp}` and `apps/node/src/main.cpp` (`onBeat`, flash bursts, text overlay, `serviceOutput`).

## Requirements

### Requirement: Shared scene table

The scenes SHALL be one compiled table shared by the Mainframe and the Node firmware, indexed from 0 in table order. Each scene SHALL have a name, one flashlight command and four RGB commands, one each for the roles wash, pulse, accent and flicker. Scene changes SHALL NOT transfer commands over the radio; a Node SHALL derive its output locally from the scene index.

#### Scenario: Same table on both sides
- **WHEN** the Mainframe selects scene 1
- **THEN** every Node renders the commands of the entry at index 1 of its own compiled copy of the table

#### Scenario: Scene commands are not sent
- **WHEN** the Mainframe changes the scene
- **THEN** it sends only the scene index and no flashlight or RGB command data

### Requirement: Node kinds and roles

A Node SHALL render as one of two kinds, flashlight or RGB strip. A flashlight Node SHALL have only the role pulse. An RGB strip Node SHALL have one of the roles wash, pulse, accent or flicker. The default role of both kinds SHALL be pulse, and a role that does not fit the kind SHALL be replaced by the default role. The UV LED kind exists only as an enum value and has no renderer.

#### Scenario: Incompatible stored role
- **WHEN** a flashlight Node is configured with the role wash
- **THEN** it uses the role pulse

#### Scenario: RGB roles
- **WHEN** an RGB strip Node is configured with the role accent
- **THEN** it keeps the role accent

### Requirement: Role-specific command derivation

For a scene index, a flashlight Node SHALL use the scene's flashlight command, whatever its role. An RGB strip Node SHALL use the scene's RGB command for its role: wash, pulse, accent or flicker. Applying a new flashlight command SHALL turn the output off, cancel any flash burst and pick the flash motif for the current bar. Applying a new RGB command SHALL restart the pattern's timing. Applying either SHALL end an output mute.

#### Scenario: RGB role picks its command
- **WHEN** an RGB strip Node with the role flicker applies scene 0
- **THEN** it renders the flicker command of scene 0

#### Scenario: All flashlights share one command
- **WHEN** two flashlight Nodes apply the same scene
- **THEN** both use the same flashlight command of that scene

#### Scenario: New command after mute
- **WHEN** a muted flashlight Node applies a flashlight command that differs from its active one
- **THEN** the mute ends and the new command is rendered

### Requirement: Beat-driven rendering

The Node SHALL render the same scene output in every run mode; only the clock source differs, as specified in `protocol`. On every local beat it SHALL pass the beat interval, beats per bar, beat in bar and bar number to its renderer and evaluate the scene's beat triggers, then advance to the next beat. Beat triggers SHALL be skipped while a visual override, a text overlay or an output mute is active.

#### Scenario: Demo and Mainframe clock
- **WHEN** a Node renders the same scene first from its demo clock and later from a Mainframe clock at the same tempo
- **THEN** it produces the same patterns at the same bar and beat positions

#### Scenario: Triggers during text
- **WHEN** a beat arrives while a text overlay is active
- **THEN** no scene flash or RGB pulse row is triggered on that beat

### Requirement: Beat trigger schedule

A beat-triggered pattern SHALL carry a trigger interval in bars and a trigger beat. A bar SHALL match when the interval is 0 or 1, or when the bar number is a multiple of the interval. A beat SHALL match when the trigger beat is 0, meaning every beat, or equals the beat in bar. The pattern SHALL fire only when bar and beat both match.

#### Scenario: Every second bar
- **WHEN** a pattern has a trigger interval of two bars and trigger beat 1
- **THEN** it fires on beat 1 of bars 2, 4, 6 and so on, and on no other beat

#### Scenario: Every beat
- **WHEN** a pattern has a trigger interval of one bar and trigger beat 0
- **THEN** it fires on every beat

### Requirement: Flash variation profile

A flashlight command SHALL be either off or a variation profile with a window length in bars, a seed and weights for five motifs: pulse, slow pulse, double pulse, quad pulse and riser. The bar number SHALL define a variation window. For each window the Node SHALL pick one motif by weight and derive its parameters, deterministically from seed and window, so Nodes at the same bar pick the same motif. With all weights 0 it SHALL pick pulse.

#### Scenario: Window change
- **WHEN** the bar number enters the next variation window
- **THEN** the Node picks the motif for that window on the first rendered beat of it

#### Scenario: Same bar on two Nodes
- **WHEN** two flashlight Nodes render the same scene at the same bar number
- **THEN** both play the same motif with the same parameters

#### Scenario: Off command
- **WHEN** the active flashlight command is off
- **THEN** the Node turns its flash output off on every beat

### Requirement: Flash motifs

Each motif SHALL resolve to a flash pattern with a trigger schedule, a burst count, a burst interval, an interval step and a flash duration. Pulse and slow pulse SHALL resolve to a single flash; double pulse, quad pulse and riser SHALL resolve to a burst of two, four and five flashes. Quad pulse and riser SHALL shorten the interval after each flash. The parameter ranges are calibration values, documented under Current values.

#### Scenario: Riser
- **WHEN** the picked motif is riser and its trigger beat arrives
- **THEN** the Node starts a burst of five flashes whose gaps get shorter

#### Scenario: Slow pulse
- **WHEN** the picked motif is slow pulse
- **THEN** the Node fires one flash on beat 1 of every second or every fourth bar

### Requirement: Single flash

When a single-flash pattern fires on a beat and its flash duration is not 0, a flashlight Node SHALL switch its flash fully on for the flash duration and then off.

#### Scenario: Beat pulse fires
- **WHEN** the motif is pulse and its trigger beat arrives
- **THEN** the flashlight lights fully for the motif's flash duration and turns off

### Requirement: Flash burst

When a burst pattern fires, a flashlight Node SHALL start a burst with its first flash at once. Before each further flash the interval SHALL change by the interval step, never below a lower limit, and the flash SHALL follow that interval after the end of the previous one. The burst SHALL end after its flash count. Bursts SHALL run from the main loop, not from the beat, and SHALL be cancelled by a new command, a valid visual state, the start of a text overlay or a mute.

#### Scenario: Quad pulse burst
- **WHEN** a quad pulse burst starts
- **THEN** the flashlight fires four flashes, each gap shorter than the one before, and then stays off

#### Scenario: Visual state during a burst
- **WHEN** a valid NodeVisualState arrives while a burst is running
- **THEN** the remaining flashes of the burst are dropped

#### Scenario: Zero-length burst
- **WHEN** a burst pattern has a flash count or flash duration of 0
- **THEN** no burst starts

### Requirement: Continuous RGB rendering

An RGB strip Node SHALL render a full frame on every main loop pass and drive all pixels of its strip from one frame buffer. A frame SHALL be built in this order: the scene pattern, or the solid directive colour in solid mode; the surface modulation in scene mode only; the directive's overlay colour blended in by its opacity; and finally the whole frame scaled by the directive's brightness.

#### Scenario: Neutral directive
- **WHEN** the visual state is scene mode at full brightness with overlay opacity 0
- **THEN** the frame shows the scene pattern with surface modulation and no further change

#### Scenario: Dimmed scene
- **WHEN** the visual state is scene mode with reduced brightness
- **THEN** the scene keeps running and every pixel is scaled down by that brightness

### Requirement: RGB command parameters

An RGB command SHALL name a pattern and carry a primary and a secondary colour, a floor, base and peak level, a trigger schedule, a start offset, timing values and, for runners, up to four bands with colour, width, phase and edge as percent of the strip. Widths and positions SHALL be relative to the whole strip, so a command does not depend on the pixel count. The off pattern SHALL render black.

#### Scenario: Off pattern
- **WHEN** an RGB strip Node renders a command with the off pattern
- **THEN** all pixels are black

### Requirement: RGB wave pattern

The wave pattern SHALL play one colour phrase per cycle of a given number of beats, starting on its start beat and positioned from bar, beat and time since the last beat. The phrase SHALL rise from black through the primary and secondary colour to a bright crest, hold it and fade to black. Each pixel SHALL be offset by a share of a travel time, so the front moves along the strip.

#### Scenario: Phrase start
- **WHEN** a wave command with a cycle of four bars and start beat 1 renders beat 1 of the first bar of a cycle
- **THEN** the phrase starts from black

#### Scenario: Missed beat
- **WHEN** no next beat arrives after one beat interval
- **THEN** the phrase holds the position it reached at the end of that interval

### Requirement: RGB pulse pattern

The pulse pattern SHALL fire on the beats of its trigger schedule. After the start offset it SHALL raise all pixels from the floor to the base level with a crest towards the peak level and back, blending from the primary towards the secondary colour, over the pulse duration, capped at one beat interval. Outside a firing beat or before the offset it SHALL show the primary colour at the floor level.

#### Scenario: Pulse on every beat
- **WHEN** a pulse command with trigger beat 0 renders a beat
- **THEN** the strip pulses once within that beat and returns to the floor level

#### Scenario: Not a trigger beat
- **WHEN** a pulse command with trigger beat 1 renders beat 2
- **THEN** the strip shows the primary colour at the floor level

### Requirement: RGB pulse row pattern

The pulse row pattern SHALL start a row on the beats of its trigger schedule, after its start offset. A row SHALL consist of a number of pulses of equal duration separated by a gap; the first pulse SHALL reach the peak level and later pulses a scaled share of it. Outside the pulses it SHALL show the primary colour at the floor level. No current scene uses this pattern.

#### Scenario: Two-pulse row
- **WHEN** a pulse row command with two pulses fires
- **THEN** the strip shows a pulse at the peak level, a gap and a weaker second pulse, then the floor level

### Requirement: RGB heartbeat and riser pulse

The heartbeat and riser pulse patterns SHALL each play a phrase of four beats that starts on the trigger beat and is positioned from bar, beat and time since the last beat. Heartbeat SHALL show two hits followed by a fading tail; riser pulse SHALL show four hits of rising strength followed by a long tail. Both SHALL light all pixels alike, between the floor and peak level, blending from the primary towards the secondary colour.

#### Scenario: Heartbeat on beat 1
- **WHEN** a heartbeat command with trigger beat 1 renders beat 1 of a bar
- **THEN** the strip shows the double hit early in the phrase and the fading tail over the rest of its four beats

### Requirement: RGB runner pattern

The runner pattern SHALL move its bands with the beat. In bounce motion each band SHALL travel from one end of the strip to the other once per beat and back on the next; in loop motion it SHALL wrap around once per beat. Parallel bands SHALL show together; sequence bands SHALL show one at a time, changing on every beat. Each pixel SHALL take the colour of its strongest band, between floor and peak level.

#### Scenario: Sequence runner
- **WHEN** a runner with two sequence bands renders two consecutive beats
- **THEN** only the first band is visible on one beat and only the second band on the next

#### Scenario: Parallel bounce runner
- **WHEN** a bounce runner with two parallel bands at different phases renders a beat
- **THEN** both bands move across the strip at the same time

### Requirement: RGB surface modulation

In scene mode an RGB strip Node SHALL modulate lit pixels with slowly changing shadows, a cool tint, a colour drift and occasional dark spots, and some patterns SHALL scale their envelopes with an activity value. All of these SHALL be derived from the Node's local time only, not from audio, radio or the Mainframe, so Nodes are not synchronised in it. Black pixels and the off pattern SHALL stay black.

#### Scenario: Two Nodes
- **WHEN** two RGB strip Nodes booted at different times render the same scene and role
- **THEN** their surface modulation may differ

#### Scenario: Off pattern
- **WHEN** the active RGB command uses the off pattern
- **THEN** no surface modulation is applied

### Requirement: Visual override

A Node SHALL treat its visual state as an override when the RGB mode is solid, the brightness is not 100 %, the overlay opacity is not 0 or the flash override is not scene. A Node SHALL start with a neutral visual state. The scene, its clock and the RGB pattern state SHALL keep running under an override, so the scene continues where it is once the override ends.

#### Scenario: Override ends
- **WHEN** an RGB strip Node receives the neutral visual state after an override
- **THEN** its next frame continues the current scene at the current bar and beat

#### Scenario: Overlay colour without opacity
- **WHEN** the visual state carries an overlay colour with opacity 0 and otherwise neutral values
- **THEN** it is not an override

### Requirement: Output precedence

Per main loop pass the output SHALL follow this precedence: a muted Node SHALL render nothing; otherwise, under a visual override, the renderer SHALL render the directive and skip flash bursts and the text overlay; otherwise an active text overlay SHALL drive the output and skip the scene; otherwise the Node SHALL service flash bursts and render the scene. An override SHALL also suppress temporary flashes and lit states requested by other parts.

#### Scenario: Text under override
- **WHEN** a text overlay is active and a visual override arrives
- **THEN** the Node renders the directive instead of the text

#### Scenario: Connect flashes under override
- **WHEN** a Node with an active visual override processes a MainframeHello
- **THEN** the flashes of the connect confirmation are not shown

#### Scenario: Text without override
- **WHEN** a text overlay is active and no visual override is set
- **THEN** the text drives the output and the scene pattern is not rendered

### Requirement: Visual override on a flashlight

Under a visual override a flashlight Node SHALL NOT start scene flashes. With flash override full it SHALL keep its flash lit; with flash override off it SHALL keep it dark. RGB mode, colour, overlay and brightness SHALL have no other effect on a flashlight, because its driver has no dimming level.

#### Scenario: Full flash
- **WHEN** a flashlight Node receives a visual state with flash override full
- **THEN** its flash stays lit until the override changes

#### Scenario: Dimmed scene on a flashlight
- **WHEN** a flashlight Node receives a visual state with reduced brightness and flash override off
- **THEN** its flash stays dark and fires no scene flashes

### Requirement: Visual override on an RGB strip

Under a visual override an RGB strip Node SHALL keep rendering every frame as specified in "Continuous RGB rendering": the solid colour replaces the scene pattern in solid mode, and overlay and brightness apply in both modes. The flash override SHALL have no light effect on an RGB strip; a value other than scene SHALL only make the visual state an override.

#### Scenario: Solid red
- **WHEN** an RGB strip Node receives solid mode with colour red at full brightness
- **THEN** all pixels show red without surface modulation

#### Scenario: Flash override on an RGB strip
- **WHEN** an RGB strip Node receives scene mode at full brightness with flash override full
- **THEN** the scene keeps rendering, and pulse row triggers and the text overlay are suppressed

### Requirement: Text overlay encoding

A Node SHALL encode a queued text as Morse light. Letters A to Z, in either case, and digits 0 to 9 SHALL be encoded; spaces between letters SHALL form a word gap; all other characters and leading spaces SHALL be skipped. A dot SHALL be one unit lit, a dash two units lit, the gap inside a letter one unit dark, between letters two units dark and between words four units dark. A text without an encodable character SHALL be ignored.

#### Scenario: Word gap
- **WHEN** the text "E E" is queued
- **THEN** the overlay is one unit lit, four units dark and one unit lit

#### Scenario: Nothing to encode
- **WHEN** a text with only punctuation is queued
- **THEN** no overlay starts and any previous overlay is cleared

### Requirement: Text overlay timing

A queued text SHALL replace any earlier overlay and start on the next local beat. A unit SHALL last half a beat; units SHALL alternate between the first and second half of a beat and SHALL be re-aligned on every beat. Starting the overlay SHALL cancel any flash burst. When the last unit has passed, the Node SHALL turn the output off and resume the scene. Queueing and cancelling are specified in `protocol`.

#### Scenario: Start on the beat
- **WHEN** a text is queued between two beats
- **THEN** its first unit starts on the next beat and lasts the first half of that beat

#### Scenario: Tempo change
- **WHEN** the beat interval changes while a text is playing
- **THEN** the following units last half of the new beat interval

#### Scenario: Text ends
- **WHEN** the last unit of a text has passed
- **THEN** the output turns off and the scene output resumes

### Requirement: Text overlay output

A flashlight Node SHALL show a lit unit as its flash fully on and a dark unit as off. An RGB strip Node SHALL show a lit unit as all pixels in a fixed white and a dark unit as black, without scene, modulation, overlay or brightness. While a visual override is active the overlay SHALL NOT drive the output and its units SHALL NOT advance by elapsed time; it SHALL continue after the override ends.

#### Scenario: RGB lit unit
- **WHEN** an RGB strip Node shows a lit unit of a text
- **THEN** all pixels show the fixed white

#### Scenario: Override during text
- **WHEN** a visual override arrives in the middle of a text and ends later
- **THEN** the text is held during the override and continues afterwards

## Current values

These are the values in the code on 2026-11-15. They are calibration and may change without a spec change. The final light design of both scenes is still open.

- Scene table: two scenes, index 0 "szene 1" and index 1 "dynamisch".
- Scene 0 flashlight: "Scene 1 Flash", window 12 bars, seed 17, weights pulse/slow pulse/double/quad/riser = 30/5/24/20/5.
- Scene 0 RGB: wash "Scene 1 Wash", wave deep blue (0,24,110) to ice blue (92,214,255), levels 0/18/152, cycle 4 bars from beat 1, travel 380 ms, crest hold 70 ms; pulse "Scene 1 Pulse", blue (0,78,255), levels 0/86/116, every beat, 380 ms; accent "Scene 1 Accent", heartbeat dark red (84,0,6) to red (255,18,0), levels 0/14/176, beat 1; flicker "Scene 1 Flicker", bounce runner with two parallel bands (deep blue and pulse blue, 24 % wide, phases 0 % and 50 %, edge 7 %), levels 0/92/176.
- Scene 1 flashlight: "Drive Flash", window 8 bars, seed 71, weights 118/35/92/45/40.
- Scene 1 RGB: wash "Drive Electric Wash", wave (5,0,45) to (0,115,175), levels 0/18/150, cycle 1 bar from beat 1, travel 1,100 ms, crest hold 40 ms, fade 900 ms; pulse "Drive Siren", loop runner with two sequence bands blue (0,28,150) and magenta-red (224,0,92), 50 % wide, no edge, levels 0/42/244; accent "Drive Riser", riser pulse (14,0,0) to (255,12,0), levels 0/44/255, beat 1; flicker "Drive Flicker", bounce runner with two parallel bands (0,96,210) and (255,0,70), 24 % wide, phases 0 % and 50 %, levels 12/92/230.
- Flash motifs (ranges are drawn per window from the seed): "Beat Pulse" every bar on every beat or beat 1, 60–85 ms; "Slow Pulse" every 2 or 4 bars on beat 1, 105–135 ms; "Double Pulse" every 1 or 2 bars on beat 1, 2 flashes of 60–85 ms, interval 330–380 ms; "Quad Pulse" every 2 or 4 bars on beat 1, 4 flashes of 45–60 ms, interval 260–310 ms, step −20 to −10 ms; "Riser 5x" every 2 or 4 bars on beat 1, 5 flashes of 40–55 ms, interval 340–410 ms, step −40 to −25 ms.
- Burst interval lower limit: 80 ms.
- Flashlight driver: GPIO 26; "on" is one preset pulse to the flash driver, "off" pulls the pin low.
- RGB strip: 15 SK6812 pixels, the same buffer output on GPIO 26 and GPIO 32; wave crest colour (148,182,232); flash for the connect confirmation full white, 140 ms when no duration is given.
- Surface modulation: slow pendulum curves with a 300 s swing plus segment noise of 36–54 s; activity 64–220, shadow depth 6–76, dark-spot chance 2–30, cool shift 2–58 (scale 0–255).
- Text overlay: one unit is half a beat, 250 ms at 120 BPM; RGB text white is level 128 of 255 on all three channels.
