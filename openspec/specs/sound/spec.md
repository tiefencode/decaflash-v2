# sound Specification

## Purpose

The sound output of the Mainframe through the Atomic Voice Base speaker: the start sound and the boot progress it drives, mood phrases on threshold crossings of the personality, snoring while the creature sleeps, and how speaker playback is kept apart from microphone capture for the audio analysis. All sounds exist only before the audio analysis is requested. The mood model, the creature battery and the sleep rules themselves belong to the personality and are referenced here, not specified. Sources: `apps/mainframe/src/sound_engine.{h,cpp}`, `apps/mainframe/src/sound_synth.{h,cpp}`, `apps/mainframe/src/sound_personality.{h,cpp}`, `apps/mainframe/src/main.cpp` (`startupSoundPreview`, `soundWatcher`), `apps/mainframe/src/voice_base_input.cpp`, `apps/mainframe/src/personality.cpp` (`consumeSnore`) `tests/sound_personality.cpp` and `tests/sound_synth.cpp`.

## Requirements

### Requirement: Locally synthesised sounds

The Mainframe SHALL synthesise every sound on the device from code and play it as raw PCM through the Voice Base speaker. It SHALL NOT load audio files and SHALL NOT need a network or external service to play a sound. The start sound SHALL be synthesised once during setup. The mood phrases and the snore SHALL each be synthesised once after boot, a small piece per main loop pass, and kept until reboot; playing one SHALL NOT synthesise it again. The Mainframe SHALL stop preparing sounds once the audio analysis is requested.

#### Scenario: Offline boot
- **WHEN** the Mainframe boots without any access point or internet connection
- **THEN** it plays the start sound

#### Scenario: Phrase played twice
- **WHEN** the same mood phrase is played a second time
- **THEN** the Mainframe plays the same prepared samples again without synthesising them

#### Scenario: Sounds prepared after boot
- **WHEN** the Mainframe has booted and the audio analysis is not requested
- **THEN** every main loop pass prepares a further piece until all phrases and the snore are ready

### Requirement: Start sound at boot

During setup the Mainframe SHALL start the speaker and schedule the start sound after a short lead-in. When the lead-in has passed it SHALL play the start sound once. When the start sound's duration has passed it SHALL stop the speaker and mark the start sound finished. If the speaker cannot start, the start sound SHALL count as finished at once; if the start sound buffer is missing or playback is refused, it SHALL count as finished at its scheduled start.

#### Scenario: Normal boot
- **WHEN** the Mainframe boots and the speaker starts
- **THEN** it plays the start sound once after the lead-in, then stops the speaker and marks the start sound finished

#### Scenario: Speaker fails to start
- **WHEN** the speaker cannot be started during setup
- **THEN** no start sound plays and the start sound counts as finished immediately

#### Scenario: Playback refused
- **WHEN** the speaker started but refuses to play the start sound at its scheduled start
- **THEN** the Mainframe stops the speaker and marks the start sound finished

### Requirement: Boot progress

The sound engine SHALL report a boot progress from 0 to 255. It SHALL be 0 until the scheduled start of the start sound, then rise linearly with elapsed time to at most 255 over the start sound's duration, and SHALL be 255 once the start sound is finished. The eye SHALL show its boot sequence while the progress is below 255 and the normal eye from 255.

#### Scenario: During the lead-in
- **WHEN** the start sound has not reached its scheduled start
- **THEN** the boot progress is 0 and the eye shows the start of its boot sequence

#### Scenario: Start sound finished
- **WHEN** the start sound is marked finished
- **THEN** the boot progress is 255 and the eye shows the normal eye

#### Scenario: No speaker
- **WHEN** the speaker could not be started during setup
- **THEN** the boot progress is 255 from the start and the eye skips its boot sequence

### Requirement: Audio analysis starts after the first press and the end of sound

The Mainframe SHALL NOT start microphone capture at boot. The first short button press SHALL start the show and request the audio analysis. The Mainframe SHALL start capture only when the analysis is requested and the sound engine reports that the start sound is finished and no other sound is playing. A playing sound SHALL NOT be cut short for this. If capture fails to start, the Mainframe SHALL try again on a later loop pass.

#### Scenario: Press during the start sound
- **WHEN** the user short-presses the button while the start sound is playing
- **THEN** the show starts at once, and microphone capture starts only after the start sound has finished and the speaker is stopped

#### Scenario: Press while a phrase plays
- **WHEN** the user short-presses the button while a mood phrase or snore is playing
- **THEN** the sound plays to its end, the speaker is stopped, and capture starts afterwards

#### Scenario: Press when no sound plays
- **WHEN** the user short-presses the button after the start sound with no sound playing
- **THEN** microphone capture starts in the same loop pass

#### Scenario: Capture fails to start
- **WHEN** the analysis is requested and the microphone or its analysis buffer cannot be started
- **THEN** the audio analysis stays off and the Mainframe tries to start it again on a later loop pass

### Requirement: Speaker and microphone are never used together

The Mainframe SHALL use the speaker only until the audio analysis is requested. After the request it SHALL NOT start any mood phrase or snore; only a sound that is already playing may still run to its end. The speaker SHALL be stopped at the end of every sound. Once capture runs, the Mainframe SHALL keep the microphone on until reboot and SHALL NOT pause it for sound output.

#### Scenario: Crossing after the request
- **WHEN** a mood crosses a threshold after the analysis was requested
- **THEN** no phrase plays

#### Scenario: Running analysis
- **WHEN** microphone capture has started
- **THEN** no sound is played until the Mainframe reboots, and capture is not paused

### Requirement: Mood threshold crossings

A watcher SHALL compare the mood with its previous value on every loop pass. For annoyance, depression, attention and loneliness it SHALL detect an UP crossing of a threshold when the value moves from below it to at or above it, and a DOWN crossing when it moves from at or above it to below it. A fired threshold direction SHALL re-arm once the value is on the other side again. The first observation SHALL NOT count as a crossing. Loneliness SHALL have no DOWN event.

#### Scenario: Rising onto a threshold
- **WHEN** annoyance moves from one point below a threshold to exactly that threshold
- **THEN** the watcher reports an UP crossing of that threshold for annoyance

#### Scenario: Leaving a band downwards
- **WHEN** attention moves from exactly a threshold to one point below it
- **THEN** the watcher reports a DOWN crossing of that threshold for attention

#### Scenario: Immediate re-arm
- **WHEN** loneliness crosses a threshold upwards, drops one point below it and rises onto it again, with the cooldown passed
- **THEN** the second rise is reported as a new UP crossing

#### Scenario: Loneliness falling
- **WHEN** loneliness falls below a threshold
- **THEN** no event is reported for loneliness

#### Scenario: First observation
- **WHEN** the watcher sees the mood for the first time after boot
- **THEN** it records the values and reports no crossing, whatever they are

### Requirement: Energy crossings

Energy SHALL have only two events: a DOWN event when it falls from above a low threshold to at or below it, and an UP event when it rises from below a high threshold to at or above it. Each event SHALL fire again only after energy has moved back past its own re-arm level, which lies between the two thresholds.

#### Scenario: Energy falls to the low threshold
- **WHEN** energy moves from one point above the low threshold to the low threshold
- **THEN** the watcher reports a DOWN crossing for energy

#### Scenario: Energy between the thresholds
- **WHEN** energy moves between thresholds that are not its low or high threshold
- **THEN** no event is reported for energy

#### Scenario: No re-arm without hysteresis
- **WHEN** energy has fired its DOWN event and rises again, but not to its low re-arm level, before falling back to the low threshold
- **THEN** no second DOWN event is reported

### Requirement: One crossing per pass

In one pass the watcher SHALL select at most one crossing: the one of the first mood with a crossing in the priority order annoyance, depression, attention, loneliness, energy. When a mood crosses several thresholds in one pass, the highest crossed threshold SHALL represent it. When the selected crossing plays, all other crossings of that pass SHALL be discarded.

#### Scenario: Two moods cross at once
- **WHEN** annoyance and attention both cross a threshold in the same pass and the annoyance phrase may play
- **THEN** only the annoyance phrase plays and the attention crossing is discarded

#### Scenario: Jump over several thresholds
- **WHEN** a mood jumps upwards across three thresholds in one pass
- **THEN** the crossing of the highest of the three is selected

### Requirement: When a mood sound may play

A selected crossing SHALL play a sound only if the analysis is not requested, the personality is not sleeping, the start sound is finished, no phrase or snore is playing and the crossing's mood has had no accepted crossing within its cooldown. Each mood SHALL have its own cooldown, started when a crossing is accepted. A crossing that is not accepted SHALL be discarded and never replayed, and its threshold SHALL stay fired.

#### Scenario: Crossing during the start sound
- **WHEN** a mood crosses a threshold while the start sound is playing
- **THEN** no phrase plays now or later for that crossing

#### Scenario: Crossing while a phrase plays
- **WHEN** a mood crosses a threshold while another phrase is playing
- **THEN** the crossing is discarded

#### Scenario: Same mood within its cooldown
- **WHEN** attention crosses a second threshold within its cooldown after an accepted attention crossing
- **THEN** no phrase plays

#### Scenario: Other mood within a cooldown
- **WHEN** attention crosses a threshold shortly after an accepted annoyance crossing and no phrase is playing
- **THEN** the attention crossing is accepted

#### Scenario: Crossing while asleep
- **WHEN** a mood crosses a threshold while the personality is sleeping
- **THEN** the crossing is discarded and no phrase plays after waking

### Requirement: Mood phrases

An accepted crossing SHALL play the phrase for its mood and direction, whatever the threshold. There SHALL be nine phrases: UP and DOWN for energy, annoyance, attention and depression, and UP for loneliness. A phrase SHALL start the speaker, play once at the phrase volume and stop the speaker when its duration has passed. If the phrase is not prepared yet, or the speaker or playback fails, no sound SHALL play, the crossing SHALL NOT be replayed and the engine SHALL stay free.

#### Scenario: Same phrase for every threshold
- **WHEN** depression crosses two different thresholds upwards at different times
- **THEN** the same depression UP phrase plays both times

#### Scenario: Phrase ends
- **WHEN** a phrase's duration has passed
- **THEN** the Mainframe stops the speaker and the engine is free for the next sound

#### Scenario: Phrase not prepared yet
- **WHEN** a crossing is accepted shortly after boot and its phrase is not prepared yet
- **THEN** no phrase plays now or later for that crossing

### Requirement: Sleep snoring

While the personality sleeps and the analysis is not requested, the Mainframe SHALL play a snore whenever the personality's snore schedule is due and the start sound is finished with no phrase or snore playing. The first snore SHALL be due a pseudo-random delay after falling asleep, and each further one a new delay after the previous snore was started. Snores SHALL NOT be queued: one due time gives at most one snore. A snore that is due before the snore is prepared SHALL be dropped. The snore SHALL be quieter than the phrases.

#### Scenario: Falling asleep
- **WHEN** the personality falls asleep before the analysis is requested
- **THEN** no snore plays until the first snore delay has passed

#### Scenario: Next snore
- **WHEN** a snore has just played and the personality keeps sleeping
- **THEN** the next snore plays after a new delay from the snore schedule

#### Scenario: Waking up
- **WHEN** the personality wakes up
- **THEN** the snore schedule is cleared and no snore plays

#### Scenario: Analysis requested
- **WHEN** the analysis has been requested
- **THEN** the Mainframe plays no snore

### Requirement: Playback does not block the loop

Playback SHALL run in the background while the main loop continues with radio, show clock, mood and display. The end of every sound SHALL be detected from elapsed time in a later loop pass. Starting a phrase or snore SHALL NOT synthesise samples, and preparing the sounds SHALL take only a bounded, short time in each loop pass.

#### Scenario: Show during the start sound
- **WHEN** the show is started while the start sound is playing
- **THEN** scene and clock sending and the display continue while the start sound plays

#### Scenario: Phrase starts
- **WHEN** an accepted crossing starts its phrase
- **THEN** the loop pass that starts it reads the motion sensor again within the usual time, so a gesture in progress is kept

## Current values

These are the values in the code on 2026-11-15. They are calibration and may change without a spec change.

- PCM format: 44,100 Hz, 8-bit unsigned, mono, quantised to a few levels with sample hold and dither for a chip-sound character. Buffers in PSRAM: start sound 105,840 bytes, prepared phrases and snore 417,627 bytes.
- Sound preparation: at most about 2 ms per loop pass in pieces of 32 samples, attention and loneliness phrases first, the snore last; all sounds ready about 61 s after boot (measured 2026-11-17).
- Start sound: lead-in 180 ms after `begin()`, duration 2,400 ms; a short original chip jingle with kick, bass, lead, arpeggio and hi-hat.
- Volume (M5 speaker scale 0–255): start sound and phrases 95 (half of 190), snore 60 (half of 120).
- Boot progress: 255 steps over the 2,400 ms start sound; the eye's white flash starts at progress 196.
- Thresholds for annoyance, depression, attention, loneliness: 10, 30, 50, 70, 90, both directions (loneliness UP only).
- Energy: DOWN at 20, re-armed at 30 or above; UP at 80, re-armed at 70 or below.
- Cooldown per mood: 10,000 ms. A mood's first sound after boot has no cooldown.
- Phrases and durations: energy UP "juHUU!" 620 ms; energy DOWN yawn into "ooou" 1,320 ms; annoyance UP "mep-mep" 1,050 ms; annoyance DOWN "mm-HM!" 400 ms; attention UP "hm?" 500 ms; attention DOWN "oh?" 500 ms; loneliness UP falling "ooou" 1,280 ms; depression UP whistled thinking melody 1,150 ms; depression DOWN pop resolution 1,400 ms.
- Snore: 1,250 ms; delay between snores 30–65 s from a fixed-seed generator (`nextSnoreDelay` in `personality.cpp`).
- Main loop: one pass about every 5 ms plus work; the mood is updated every 100 ms.
