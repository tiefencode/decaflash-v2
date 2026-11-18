# personality Specification

## Purpose

The local personality of the Mainframe: five mood values, how audio features, tempo estimates, motion events and time change them, the creature battery with sleep and waking before the audio analysis is requested, the snapshot other parts read, the mapping from mood to the Node light directive, and the mood debug overlay. How the eye, the message panel and the sounds react to the mood is specified in the eye-iris, message-panel and sound specs; packet layout and sending of the light directive are in the protocol spec. The classification of IMU data into motion events and the tempo tracker are not part of this spec. Sources: `apps/mainframe/src/personality.{h,cpp}`, `apps/mainframe/src/audio_mood_features.{h,cpp}`, `moodAudio()`, `visualStateForMood()` and the `personality` calls in `apps/mainframe/src/main.cpp`, the debug branch of `EyeRenderer::service()` in `apps/mainframe/src/eye_renderer.cpp`, `tests/personality.cpp` and `tests/audio_mood_features.cpp`.

## Requirements

### Requirement: Five local mood values

The Mainframe SHALL keep five mood values, energy, annoyance, attention, loneliness and depression, each limited to 0–100 and kept internally at a finer resolution than whole points. The model SHALL be deterministic and run on the Mainframe alone, without network, cloud or language model. The values SHALL NOT be stored across a restart; after every boot they start from fixed start values.

#### Scenario: Restart
- **WHEN** the Mainframe reboots
- **THEN** every mood value starts again from its start value, whatever it was before

#### Scenario: Offline
- **WHEN** the Mainframe runs without access point or internet
- **THEN** the mood values change by the same rules as with a network

### Requirement: Update cadence

The main loop SHALL advance the personality at a fixed update interval and additionally right before it applies a motion event, so the time-driven changes are up to date when the event lands. Each update SHALL integrate the time since the previous one. Rounding remainders SHALL be carried over, so the result does not depend on how often updates run. Time differences SHALL stay correct across the `millis()` overflow.

#### Scenario: Fine and coarse updates
- **WHEN** one personality is updated every millisecond and another every 100 ms with the same inputs for 30 s
- **THEN** both report the same energy, loneliness and depression

#### Scenario: Timer overflow
- **WHEN** updates run across the overflow of the millisecond counter
- **THEN** the elapsed time is computed as if no overflow happened

### Requirement: Long gaps are not caught up

When more time than the maximum integration step has passed since the previous update, the personality SHALL integrate only the maximum step. Such a gap SHALL also restart the silence confirmation and discard the collected tempo-trust estimates.

#### Scenario: Blocked loop
- **WHEN** five seconds pass between two updates
- **THEN** the mood changes as if only the maximum integration step had passed, and silence and tempo trust start collecting again

### Requirement: Loneliness rises over time

Loneliness SHALL rise at a constant rate up to 100 in every state: before and after the analysis is requested, awake or asleep. Only motion events SHALL lower it. Audio SHALL NOT change loneliness.

#### Scenario: Nobody touches the device
- **WHEN** no motion event occurs for ten minutes
- **THEN** loneliness reaches 100 and stays there

#### Scenario: Music does not help
- **WHEN** the same music plays at a slow and at a fast tempo for the same time without motion
- **THEN** loneliness is the same in both cases

### Requirement: Annoyance and attention fade

Annoyance and attention SHALL move back towards 0 at constant rates whenever time passes, attention much faster than annoyance. Only motion events SHALL raise them.

#### Scenario: Attention after a tap
- **WHEN** a tap raises attention and one second passes without further events
- **THEN** attention has dropped by the attention fade rate for one second

#### Scenario: Annoyance after an impact
- **WHEN** an impact raises annoyance and no further event follows
- **THEN** annoyance sinks slowly and reaches 0 after the time its fade rate needs

### Requirement: Motion events change the social values

Each motion event SHALL add a fixed amount per event kind to attention and annoyance and subtract a fixed amount from loneliness, saturating at 0 and 100. MultiTap SHALL add annoyance only from three taps. Reposition SHALL change no value. Motion SHALL NOT change energy or depression directly. The amounts are listed under Current values.

#### Scenario: Tap
- **WHEN** a Tap event arrives
- **THEN** attention rises, loneliness falls, and annoyance and energy stay unchanged

#### Scenario: Double tap
- **WHEN** a MultiTap event with two taps arrives
- **THEN** annoyance stays unchanged

#### Scenario: Reposition
- **WHEN** a Reposition event arrives
- **THEN** no mood value changes

### Requirement: Audio state for the mood

Before the analysis has started, and whenever no microphone block was processed within the freshness window, the audio state SHALL be unknown. Unknown SHALL never count as silence and SHALL supply no audio contribution to any mood. Otherwise the state SHALL be silent or sound, taken from the silence detection.

#### Scenario: Capture stops
- **WHEN** the microphone delivers no block for longer than the freshness window
- **THEN** the audio state is unknown, not silent

### Requirement: Silence detection

The Mainframe SHALL judge silence per microphone block from the RMS level after a high-pass filter, with a lower level to enter silence than to leave it. Silence detection SHALL be independent of the adaptive music gate of the tempo path. A capture gap longer than the gap limit SHALL reset it to not silent.

#### Scenario: Level between the two limits
- **WHEN** the block level lies between the entry and the exit level
- **THEN** the previous silent or not-silent state is kept

#### Scenario: Constant offset only
- **WHEN** the microphone delivers a constant DC offset without any signal
- **THEN** the block counts as silent

### Requirement: Bass share

The Mainframe SHALL estimate the share of low-frequency power in the high-passed signal with simple filters, without FFT, smoothed over a long time constant. The share SHALL be valid only after enough usable samples. A silent block, a block with too many clipped samples or a capture gap longer than the gap limit SHALL discard the collected bass history. The share SHALL not depend on loudness.

#### Scenario: Warm-up
- **WHEN** music has been captured for less than the warm-up time
- **THEN** the bass share is not valid

#### Scenario: Loud and soft bass tone
- **WHEN** the same low tone is captured at two different levels
- **THEN** both give nearly the same bass share

#### Scenario: Clipping
- **WHEN** a block has more clipped samples than the clipping limit
- **THEN** the bass share becomes invalid and collects again from the next usable block

### Requirement: Tempo trust

An estimate of the tempo tracker SHALL count as reliable when the audio state is sound, its BPM lies in the accepted range, its confidence reaches the confidence threshold and its last onset is recent. A tempo SHALL become trusted after three reliable estimates in a row, each with a new onset, that stay close to their running average. A trusted tempo SHALL stay usable for a hold time after it was last confirmed and only while the audio state is sound.

#### Scenario: Steady track
- **WHEN** three reliable estimates with new onsets report nearly the same BPM
- **THEN** their average becomes the trusted tempo

#### Scenario: Tempo jump
- **WHEN** a reliable estimate differs from the running average by more than the allowed distance
- **THEN** collection starts again from that estimate

#### Scenario: Unreliable update
- **WHEN** any update in between sees no reliable estimate
- **THEN** the collected estimates are discarded

### Requirement: Energy follows the tempo

After the analysis is requested, while the audio state is sound and the tracker BPM lies in the accepted range, energy SHALL move towards a target from the current, unsmoothed tracker BPM, limited to the energy BPM range and rising more steeply at high tempo. A current, confident beat SHALL add a small bonus growing with confidence. Energy SHALL fall towards a lower target faster than it rises. Loudness SHALL NOT raise energy.

#### Scenario: Fast track with clear beats
- **WHEN** a 160 BPM track with confident beats plays long enough
- **THEN** energy settles at the 160 BPM base plus the beat bonus for that confidence

#### Scenario: Tempo without a confident beat
- **WHEN** the tracker reports 160 BPM with confidence below the threshold
- **THEN** energy settles at the 160 BPM base without bonus

#### Scenario: Slower song
- **WHEN** the tempo drops from 160 to 115 BPM
- **THEN** energy falls at the fast falling rate

### Requirement: Energy on silence

After the analysis is requested, when silence has lasted the silence delay without interruption, energy SHALL fall towards 0 at a fixed rate. A block that is no longer silent SHALL stop the fall at once. Silence shorter than the delay SHALL leave energy unchanged.

#### Scenario: Music stops
- **WHEN** silence starts and continues for several seconds
- **THEN** energy stays unchanged for the silence delay and then falls to 0

#### Scenario: Music returns
- **WHEN** sound returns while energy is falling on silence
- **THEN** the fall stops in the same update

### Requirement: Energy holds without evidence

After the analysis is requested, energy SHALL keep its value while the audio state is unknown, and while sound is present but the tracker reports no BPM in the accepted range.

#### Scenario: Capture failure
- **WHEN** capture delivers no fresh blocks
- **THEN** energy stays at its last value and does not fall as on silence

#### Scenario: No tempo estimate
- **WHEN** sound is present but the tracker BPM is outside the accepted range
- **THEN** energy stays at its last value

### Requirement: Creature battery before the analysis

Until the audio analysis is requested by the first short button press, energy SHALL be a creature battery instead of a music value. On entering this mode at boot the battery SHALL be full. While awake it SHALL drain linearly to 0 over the drain time. Audio SHALL NOT affect it.

#### Scenario: Boot
- **WHEN** the Mainframe boots and the personality is first updated
- **THEN** energy is 100 and the creature is awake

#### Scenario: Drain
- **WHEN** the creature stays awake without the analysis being requested
- **THEN** energy falls linearly and reaches the tired level after the matching share of the drain time

### Requirement: Falling asleep

While the creature is awake and the battery is at or below the tired level, a grace timer SHALL run. Every motion event while awake SHALL restart it. When the grace time passes without a motion event, the creature SHALL fall asleep. At an empty battery it SHALL fall asleep at once.

#### Scenario: Tired and untouched
- **WHEN** the battery is at the tired level and no motion event occurs for the grace time
- **THEN** the creature falls asleep

#### Scenario: Tired and handled
- **WHEN** a motion event arrives while the grace timer runs
- **THEN** the timer restarts and the creature stays awake

#### Scenario: Turned away
- **WHEN** the device is left turned away, so Reposition events repeat while the battery is at or below the tired level
- **THEN** each Reposition event restarts the timer, and once the motion classifier has learned the new direction and the repeats stop, the creature can fall asleep after the grace time

### Requirement: Sleep and waking

While asleep, the battery SHALL recharge linearly to full over the recharge time and the other moods SHALL keep changing by their rules. A motion event SHALL wake the creature only once the battery has reached the wake level; below it the event changes the moods but not the sleep. A full battery SHALL wake the creature on its own. The snore schedule kept while asleep is specified in the sound spec.

#### Scenario: Tap right after falling asleep
- **WHEN** a Tap event arrives while asleep and below the wake level
- **THEN** the creature keeps sleeping, and attention and loneliness change as for any tap

#### Scenario: Tap after a short nap
- **WHEN** a Tap event arrives while asleep and at or above the wake level
- **THEN** the creature wakes up

#### Scenario: Full recharge
- **WHEN** the battery reaches 100 during sleep
- **THEN** the creature wakes up without any event

### Requirement: Leaving creature mode

When the audio analysis is requested, the creature mode SHALL end for the rest of the run: a sleeping creature SHALL wake at once, sleep SHALL no longer occur, and energy SHALL keep its current battery value until the music rules change it.

#### Scenario: Request while asleep
- **WHEN** the first short button press comes while the creature sleeps
- **THEN** the creature is awake in the next update and energy keeps the battery value

### Requirement: Depression from the social values

Depression SHALL change only while an influence is present. Loneliness above the social threshold SHALL raise it at a rate growing linearly up to a maximum at 100. Attention above the same threshold SHALL lower it in the same way. These social terms SHALL apply in every state, before and after the analysis is requested. Without any influence depression SHALL keep its value.

#### Scenario: Long loneliness
- **WHEN** loneliness stays at 100 for a long time without music
- **THEN** depression rises steadily

#### Scenario: Strong attention
- **WHEN** attention is above the social threshold
- **THEN** depression falls by the amount that loneliness at the same level would add

#### Scenario: Nothing happens
- **WHEN** loneliness and attention are at or below the social threshold and the audio state is unknown
- **THEN** depression stays unchanged

### Requirement: Depression from music

While the audio state is sound, music SHALL lower depression at a small constant rate. A usable trusted tempo above the neutral range SHALL add to it, most at the peak tempo and nothing at the upper end of the BPM range. A valid low bass share SHALL add to it, linearly more the lower the share. These terms can outweigh the recovery. Silence and unknown audio SHALL supply none of them.

#### Scenario: Neutral tempo
- **WHEN** a trusted tempo of 80 or 100 BPM plays with ample bass
- **THEN** depression does not rise

#### Scenario: Slow tempo against fast tempo
- **WHEN** the same music runs for minutes once at 120 BPM and once at 160 BPM
- **THEN** depression ends higher at 120 BPM

#### Scenario: Weak bass without beat
- **WHEN** sound with a valid, very low bass share but no tempo plays for two minutes
- **THEN** depression rises

### Requirement: Mood snapshot

Other parts SHALL read the mood only as a snapshot of whole points, each internal value rounded down, together with the sleep state. The main loop SHALL take one snapshot per pass after the personality updates and pass it to the depression poems, the mood sounds, the Node light directive, the eye and the debug overlay. For a motion or presence event, the text bank's selection and the text model request SHALL read a snapshot taken after the personality has applied that event. No other part, the text model included, SHALL change the mood.

#### Scenario: Fraction below a threshold
- **WHEN** a mood value is internally just below 90
- **THEN** the snapshot reports 89

#### Scenario: One pass, one view
- **WHEN** the eye, the sounds and the light directive read the mood in the same loop pass
- **THEN** they all see the same snapshot

#### Scenario: Text after an impact
- **WHEN** an Impact raises annoyance and the panel takes a text for it
- **THEN** the text bank chooses the lead value from a snapshot that already includes the raised annoyance

#### Scenario: Text after a tap
- **WHEN** a Tap raises annoyance and the panel requests a model text for it
- **THEN** the request carries the snapshot taken after the Tap was applied to the personality

### Requirement: Mood to Node light directive

The Mainframe SHALL derive the Node light directive from the snapshot alone. At or above the light threshold, annoyance SHALL give solid red at full brightness with the flash permanently on. Otherwise loneliness at or above the light threshold SHALL keep the scene colours, dim them and switch the flash off. Otherwise the directive SHALL be neutral. Annoyance SHALL take precedence. Mood values SHALL NOT be sent to Nodes.

#### Scenario: Very annoyed
- **WHEN** annoyance reaches the light threshold
- **THEN** the directive is solid red at full brightness with flash override full

#### Scenario: Very lonely
- **WHEN** loneliness is at the light threshold and annoyance below it
- **THEN** the directive keeps scene mode, sets the dimmed brightness and switches the flash off

#### Scenario: Both high
- **WHEN** annoyance and loneliness are both at or above the light threshold
- **THEN** the annoyance directive is used

#### Scenario: Calm
- **WHEN** both annoyance and loneliness are below the light threshold
- **THEN** the directive is scene mode at full brightness, no overlay and flash override scene

### Requirement: Mood debug overlay toggle

A long press of the front button SHALL toggle the mood debug overlay, once per press when the hold time is reached; a long press SHALL NOT also count as a short press. The overlay SHALL replace the message panel and SHALL be drawn over the eye, during sleep and during the boot sequence as well. The button itself SHALL NOT change any mood value.

#### Scenario: Long press
- **WHEN** the button is held past the hold time while the overlay is hidden
- **THEN** the overlay appears and the show and scene stay unchanged

#### Scenario: Debugging while asleep
- **WHEN** the overlay is on while the creature sleeps
- **THEN** the overlay is drawn over the sleeping eye

### Requirement: Mood debug overlay content

The overlay SHALL show the five snapshot values as labelled numbers with proportional bars, the kind and count of the latest motion event, the show BPM and the tracker's candidate BPM. It SHALL leave the beat dot visible.

#### Scenario: Before any motion
- **WHEN** the overlay is shown before any motion event was classified
- **THEN** the motion field shows a placeholder instead of an event kind

#### Scenario: Tempo readout
- **WHEN** the show clock runs at 120 BPM and the tracker estimates 124 BPM
- **THEN** the overlay shows `120/124`

## Current values

These are the values in the code on 2026-11-15. They are calibration and may change without a spec change.

- Start values: energy 30, annoyance 0, attention 0, loneliness 50, depression 0. In the firmware the first update runs in creature mode and sets energy to 100, so 30 is not visible.
- Update interval: 100 ms (`main.cpp`); IMU polled every 20 ms. Maximum integration step: 1,000 ms.
- Loneliness: +0.1 points/s. Annoyance: −0.1 points/s. Attention: −3 points/s.
- Motion amounts (attention / annoyance / loneliness): Move +8 / 0 / −5; Tap +12 / 0 / −8; MultiTap +18 / +6 from three taps, else 0 / −12; Impact +20 / +15 / −10; Rotate +10 / 0 / −6; Reposition 0 / 0 / 0; Shake +25 / +10 / −15.
- Audio freshness window: 250 ms (`VoiceBaseInput::fresh`). Capture gap limit for silence and bass: 250 ms.
- Silence: RMS of the high-passed 16-bit block below 80 enters silence, 120 or above leaves it. Silence delay: 1 s; energy then falls at 20 points/s.
- Bass share: one-pole high-pass near 40 Hz, two low-pass poles near 180 Hz on 16 kHz PCM; smoothing time constant and warm-up 240,000 samples (15 s). Clipping: a sample at ±32,700 or beyond in more than 1 % of a block.
- Tempo reliability: BPM 60–200, confidence ≥ 68 (see BUG-003), last onset at most 1,500 ms old; onsets more than 1,500 ms apart restart the collection. Allowed distance from the running average: 4 BPM; running average = (average + new) / 2. Trusted tempo hold time: 15 s.
- Energy: tracker BPM accepted 60–200, limited to 80–160 for the target. Base: 2/3 point per BPM above 60 up to 120 BPM (80 → 13, 100 → 26, 120 → 40), then 1 point per BPM to 140 (60), then 1.5 points per BPM to 160 (90). Beat bonus when confidence ≥ 68 and the last onset is at most 1,500 ms old: (confidence − 68) × 10 / 32, so 10 at confidence 100. Rising 4 points/s, falling 20 points/s.
- Creature battery: full 100; drain 100 points in 600 s; tired level 20; grace time 10 s; wake level 25; recharge 100 points in 300 s.
- Depression: social threshold 80, social rate up to 0.2 points/s at 100. Music recovery −0.05 points/s. Trusted tempo limited to 80–160: neutral up to 100 BPM, rising to +0.1 points/s at 120 BPM, falling to 0 at 160 BPM. Bass: +0.12 points/s at a share of 8 % or less, falling linearly to 0 at 25 %.
- Light directive: light threshold 90 for annoyance and loneliness. Annoyance: RGB mode solid, colour 255/0/0, brightness 100 %, flash override full. Loneliness: RGB mode scene, brightness 15 %, flash override off. Neutral: RGB mode scene, brightness 100 %, overlay opacity 0, flash override scene.
- Debug overlay: hold time 1,200 ms (`M5.BtnA.setHoldThresh`). Motion placeholder `--`. Tempo readout `show/candidate` at the top, beat dot area at the top right kept free.
