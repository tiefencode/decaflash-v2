# tempo Specification

## Purpose

The productive audio input, tempo tracking and show clock of the Mainframe: microphone capture from the Atomic Voice Base, the analysis ring and its sample-clock time base, audio freshness, the PCM tempo tracker, the confirmation stage that adopts a measured tempo into the show, and the show clock with its beats and bars. What the Mainframe sends over the radio, and when, is specified in the protocol spec; when capture may start relative to sound output is specified in the sound spec; the VU level is specified in the eye-iris spec, the beat dot in the eye-iris spec, the tempo readout in the debug-views and personality specs, and how tempo changes the mood in the personality spec. Octave handling, holdover and phase alignment are open decisions that the user settles only after hardware measurements (docs/project-context.md, section 0); the requirements below describe the current behaviour only and are not confirmed as intended. The old modules `beat_analyzer`, `audio_follower`, `bpm_tracker` and `spectral_onset_features` are not part of the productive path and are not specified. Sources: `apps/mainframe/src/voice_base_input.{h,cpp}`, `apps/mainframe/src/v2_tempo_tracker.{h,cpp}`, `apps/mainframe/src/v2_tempo_adoption.h`, `apps/mainframe/src/show_clock.h`, `apps/mainframe/src/main.cpp` (`startShow`, `selectNextScene`, `applyV2Tempo`, `serviceShowClock`, `loop`) and `tests/v2_tempo_tracker.cpp`, `tests/v2_tempo_adoption.cpp`, `tests/show_clock.cpp`.

## Requirements

### Requirement: Microphone capture

The Mainframe SHALL capture mono 16-bit PCM from the Voice Base microphone through M5Unified at a fixed sample rate in blocks of a fixed sample count. Two capture buffers SHALL be queued alternately; the capture callback SHALL queue each released buffer again itself, without waiting for the main loop. Capture SHALL start only when the sound spec allows it and SHALL then run until reboot.

#### Scenario: Continuous capture
- **WHEN** capture has started and the main loop is busy drawing a frame
- **THEN** the capture callback keeps queuing the two buffers and capture continues without waiting for the main loop

### Requirement: Capture start

Starting capture SHALL allocate the analysis ring in PSRAM, configure and start the microphone and queue both capture buffers. If the ring cannot be allocated, the microphone is not enabled or does not start, or a buffer cannot be queued, the start SHALL fail, release what it set up and leave capture not ready. A new start SHALL reset the analysis ring, its counters, the tempo tracker and the other block consumers.

#### Scenario: PSRAM allocation fails
- **WHEN** the analysis ring cannot be allocated
- **THEN** the start fails, the microphone is not started and capture stays not ready

#### Scenario: Buffer cannot be queued
- **WHEN** the microphone starts but one of the two capture buffers cannot be queued
- **THEN** the microphone is stopped, the ring is released and capture stays not ready

### Requirement: Analysis ring

The capture callback SHALL copy each finished block into a ring in PSRAM together with its audio timestamp and a sequence number. On every pass the main loop SHALL process all queued blocks in capture order. When the ring is full, the callback SHALL drop the new block, count the drop and still advance the sequence number and the sample count. It SHALL record the highest backlog seen.

#### Scenario: Short main-loop pause
- **WHEN** the main loop pauses for less time than the ring holds
- **THEN** no block is lost and the next pass processes all queued blocks in order

#### Scenario: Ring overflow
- **WHEN** the main loop pauses longer than the ring holds
- **THEN** the blocks that do not fit are dropped and counted, and capture keeps running

### Requirement: Requeue failure recovery

If the capture callback cannot queue a released buffer again, it SHALL count the failure and mark the buffer. The main loop SHALL try to queue every marked buffer on each pass until it succeeds.

#### Scenario: Requeue fails in the callback
- **WHEN** the callback fails to queue a buffer
- **THEN** the failure is counted and the main loop queues that buffer on a later pass

### Requirement: Sample-clock time base

Each block's audio timestamp SHALL be the time capture started plus the duration of all blocks captured so far, dropped blocks included, computed from the sample count at the nominal sample rate. The tempo tracker SHALL receive this audio timestamp, not the time at which the main loop processes the block.

#### Scenario: Late processing
- **WHEN** the main loop processes several queued blocks in one pass after a pause
- **THEN** their audio timestamps stay one block duration apart, as when they were captured

### Requirement: Capture discontinuity resets the tracker

When a processed block's sequence number does not directly follow the previous one, the Mainframe SHALL reset the tempo tracker before feeding the block. The tracker SHALL also reset itself when the audio timestamps of two consecutive blocks are further apart than its gap limit. After a reset the tracker SHALL report BPM 0 and confidence 0 until it has collected enough gapless history again.

#### Scenario: Dropped block
- **WHEN** a block was dropped because the ring was full
- **THEN** the next processed block resets the tempo tracker, which starts collecting history again

#### Scenario: Timestamp gap
- **WHEN** the tracker has a tempo estimate and receives a block whose timestamp is far beyond the gap limit after the previous block
- **THEN** it reports BPM 0, and that block is the first of its new history

### Requirement: Audio freshness

Audio SHALL count as fresh only while capture is ready, at least one block has been processed and the last block was processed by the main loop within the freshness window, measured in main-loop time. Tempo adoption and the personality SHALL use this freshness.

#### Scenario: Blocks stop arriving
- **WHEN** capture is ready but no block has been processed for longer than the freshness window
- **THEN** audio is not fresh

#### Scenario: Before capture starts
- **WHEN** capture has not been started
- **THEN** audio is not fresh

### Requirement: One capture feeds all audio consumers

Every processed block SHALL be fed, in this order, to the tempo tracker, the audio mood features of the personality and, as the DC-corrected mean absolute level of the block, the iris VU. No consumer SHALL have its own microphone capture.

#### Scenario: One block
- **WHEN** the main loop processes one block
- **THEN** the tempo tracker, the mood features and the VU each receive that same block once

### Requirement: Onset strength

For each block the tracker SHALL compute one onset strength: the largest of the broadband level rise, the bass level rise and the four-band log-energy flux. Both level rises SHALL be divided by the current level, bounded below by a noise floor, so source volume does not set the beat threshold. There SHALL be no music gate and no absolute onset threshold. The tracker SHALL use no FFT and no extra library.

#### Scenario: Bass kick
- **WHEN** a block contains the start of a bass kick after a quiet block
- **THEN** the onset strength of that block rises above that of the quiet block

#### Scenario: Quieter source
- **WHEN** the same kick pattern plays at a lower volume well above the noise floor
- **THEN** the level-rise contributions to the onset strength stay about the same

### Requirement: Individual onsets

The tracker SHALL mark an onset at a block whose onset strength is a local maximum above an adaptive threshold of running mean plus a multiple of the running mean deviation, unless it lies within the refractory time after the previous onset. It SHALL keep the timestamps of a fixed number of recent onsets and expose the timestamp of the last one. Onsets SHALL be detected from the first block on.

#### Scenario: Two peaks close together
- **WHEN** two onset-strength peaks above the threshold lie closer together than the refractory time
- **THEN** only the first is marked as an onset

#### Scenario: Steady kick train
- **WHEN** a steady kick train plays
- **THEN** the last onset timestamp moves forward with each kick

### Requirement: Tempo evaluation schedule

The tracker SHALL evaluate the tempo only once its gapless onset-strength history has reached a minimum length, and then at a fixed interval of audio time. Each evaluation SHALL increase an evaluation counter. Between evaluations the published estimate SHALL stay unchanged.

#### Scenario: Not enough history
- **WHEN** less than the minimum history has been collected since the start or the last reset
- **THEN** the tracker reports BPM 0, confidence 0 and no evaluation has run

#### Scenario: Regular evaluations
- **WHEN** gapless audio keeps arriving after the minimum history is reached
- **THEN** a new evaluation runs once per evaluation interval of audio time

### Requirement: Periodicity search

An evaluation SHALL compute the normalised autocorrelation of the decimated onset-strength history for candidate tempi across the search range: a coarse scan, then a fine scan around the best candidate. The best-scoring tempo SHALL be the correlation winner. If no candidate has a positive correlation, the evaluation SHALL publish BPM 0 and confidence 0.

#### Scenario: Clear kick train
- **WHEN** a clear kick train at 128 BPM has played for 12 seconds
- **THEN** the tracker reports between 126 and 130 BPM with a non-zero confidence

#### Scenario: No periodicity
- **WHEN** no candidate tempo has a positive correlation
- **THEN** the evaluation publishes BPM 0 and confidence 0

### Requirement: Double-tempo check

When the correlation winner is at most half the top of the search range, the tracker SHALL also score its double tempo. It SHALL replace the winner with the double tempo only when the recent onset intervals support the double tempo by a minimum margin more than the winner and the double tempo's correlation reaches a minimum fraction of the winner's.

#### Scenario: Intervening beats present
- **WHEN** the correlation favours 81 BPM, but the onsets between the kicks make the intervals match 162 BPM clearly better and 162 BPM still correlates well enough
- **THEN** the evaluation continues with 162 BPM

#### Scenario: Weak double tempo
- **WHEN** the onset intervals favour the double tempo but its correlation is below the minimum fraction
- **THEN** the evaluation keeps the correlation winner

### Requirement: Direct onset tempo

The tracker SHALL derive a direct tempo from the mean of the most recent onset intervals when enough onsets exist, every interval lies in the allowed range and none deviates from the mean by more than the allowed share. If the direct tempo lies in the output range and is within a tolerance of the evaluation result, its double or its half, it SHALL replace the result.

#### Scenario: Direct tempo at half the result
- **WHEN** the evaluation result is 180 BPM and steady recent onsets give a direct tempo of 90 BPM
- **THEN** the evaluation publishes 90 BPM

#### Scenario: Irregular onsets
- **WHEN** one of the recent onset intervals deviates from their mean by more than the allowed share
- **THEN** there is no direct tempo and the result stays as it was

### Requirement: Tempo confidence

Each evaluation SHALL publish a confidence from 0 to 100, combining the correlation score of the result with its separation from the best competing coarse candidate at least a few BPM away from the published tempo. When the direct onset tempo matched and replaced the result, the confidence SHALL be at least a fixed floor. The confidence SHALL be a heuristic, not a calibrated probability.

#### Scenario: Direct agreement
- **WHEN** the direct onset tempo agrees with the evaluation result but periodicity and separation are low
- **THEN** the published confidence is the floor value

### Requirement: No lock in the tracker

Each evaluation SHALL publish its result as the current BPM and confidence at once. The tracker SHALL have no tempo lock, no song-change detection and no state that keeps an older tempo alive.

#### Scenario: Tempo change in the music
- **WHEN** a 100 BPM kick train is followed without a gap by a 120 BPM kick train for 16 seconds
- **THEN** the tracker reports between 118 and 122 BPM without any reset

### Requirement: Adoption input

While the show runs and audio is fresh, the Mainframe SHALL pass each tracker evaluation to the adoption stage once. The stage SHALL reject an evaluation and clear any pending candidate when its BPM is outside the adoption range, its confidence is below the adoption threshold, there is no onset or the last onset is older than the onset age limit. An evaluation equal to the show tempo SHALL also clear the candidate.

#### Scenario: Same evaluation seen twice
- **WHEN** the main loop passes the same evaluation to the adoption stage on two passes
- **THEN** the second pass has no effect

#### Scenario: Low confidence
- **WHEN** a candidate is pending and the next evaluation has a confidence below the adoption threshold
- **THEN** the candidate is cleared and the show tempo stays

#### Scenario: Stale audio
- **WHEN** audio is not fresh
- **THEN** no evaluation reaches the adoption stage and the show tempo stays

### Requirement: Candidate confirmation

An accepted evaluation that differs from the show tempo SHALL become the candidate when no candidate is pending or it is further from the candidate than the candidate distance. When the next accepted evaluation is within that distance, the stage SHALL adopt the rounded mean of candidate and evaluation and clear the candidate. The adopted mean SHALL NOT be compared with the show tempo again.

#### Scenario: Single outlier
- **WHEN** the show runs at 101 BPM and one evaluation reports 134 BPM, followed by two evaluations of 99 BPM
- **THEN** 134 BPM is not adopted and 99 BPM is adopted on the second 99 BPM evaluation

#### Scenario: Two close evaluations
- **WHEN** the show runs at 99 BPM and two consecutive evaluations report 120 and 121 BPM
- **THEN** the show adopts 121 BPM

#### Scenario: Mean equals the show tempo
- **WHEN** the show runs at 120 BPM and two consecutive evaluations report 118 and 122 BPM
- **THEN** the stage adopts 120 BPM

#### Scenario: Half or double tempo
- **WHEN** two consecutive evaluations report half the show tempo, inside the adoption range and with sufficient confidence
- **THEN** the half tempo is adopted, because the stage does not relate tempi by octave

### Requirement: Applying an adopted tempo

On an adoption the Mainframe SHALL set the show tempo, move the show clock to beat 1 of the next bar and send a ClockSync as defined in the protocol spec. It SHALL schedule the next beat at the last onset plus a whole number of beat intervals, the first such time after now, and mark the beat as a sync beat. The bar position SHALL NOT be derived from the music.

#### Scenario: Adoption in the middle of a bar
- **WHEN** the show is on beat 3 of bar 20 and a tempo is adopted
- **THEN** the show tempo changes, the clock is on beat 1 of bar 21, a ClockSync for that position is sent and the next beat is beat 2

#### Scenario: Phase from the last onset
- **WHEN** a tempo is adopted and the last onset lies less than one new beat interval in the past
- **THEN** the next beat is due one beat interval after that onset

### Requirement: Show keeps its last tempo

The show tempo SHALL change only through an adoption. During silence, a break, stale audio, a tracker reset or a run of rejected evaluations the show SHALL keep running at its last tempo. It SHALL NOT fall back to the start tempo and SHALL have no holdover or search state.

#### Scenario: Music stops
- **WHEN** the show runs at an adopted 128 BPM and the music stops
- **THEN** the show clock keeps running at 128 BPM

#### Scenario: Capture gap during the show
- **WHEN** a capture discontinuity resets the tracker while the show runs
- **THEN** the show tempo stays until a new tempo is adopted

### Requirement: Show start

The first short press of the front button outside debug mode SHALL start the show and request the audio analysis. Starting SHALL clear the adoption stage, set the clock to bar 1, beat 1 with four beats per bar at the start tempo, schedule the next beat one beat interval later, send SceneSelect and ClockSync as defined in the protocol spec and mark a sync beat. The show SHALL keep running until reboot; there is no stop.

#### Scenario: First press
- **WHEN** the user short-presses the front button outside debug mode before the show runs
- **THEN** the show runs at the start tempo on bar 1, beat 1, and the audio analysis is requested

#### Scenario: Before capture delivers a tempo
- **WHEN** the show has started and no tempo has been adopted yet
- **THEN** the show clock runs at the start tempo

### Requirement: Free-running show clock

While the show runs, the main loop SHALL advance the clock by one beat when the next beat is due and schedule the following beat one beat interval after that beat's due time. Beats that became due while the loop was busy SHALL all be counted at once. After the last beat of a bar it SHALL move to beat 1 of the next bar and send a ClockSync as defined in the protocol spec. A beat-in-bar value beyond the bar length SHALL also move to beat 1 of the next bar.

#### Scenario: Bar after the show start
- **WHEN** the show has started on bar 1, beat 1 and four beats have become due
- **THEN** the clock has shown beats 2, 3 and 4 of bar 1 and is on beat 1 of bar 2, and only that beat sent a ClockSync

#### Scenario: Beat noticed late
- **WHEN** the main loop notices a due beat some milliseconds after its due time
- **THEN** the following beat is due one beat interval after the due time of the noticed beat, so the lateness is not passed on to later beats

#### Scenario: Whole beats missed
- **WHEN** the show is on beat 3 of a bar and the main loop next runs after three further beats have become due
- **THEN** the clock is on beat 2 of the next bar, and the following beat is due on the unchanged beat grid

### Requirement: Scene change keeps the clock

A short press of the front button outside debug mode while the show runs SHALL select the next scene as defined in the protocol spec. It SHALL NOT change the show tempo, the beat, the bar or the schedule of the next beat.

#### Scenario: Scene change on beat 3
- **WHEN** the show is on beat 3 of a bar and the user short-presses the button outside debug mode
- **THEN** the next scene is selected and the following beat is still beat 4 at its scheduled time

## Current values

These are the values in the code on 2026-11-16. They are calibration and may change without a spec change.

- Capture: 16,000 Hz, 16-bit mono, blocks of 256 samples (16 ms); M5Unified microphone with 4 DMA buffers of 256 samples, no oversampling, magnification 1, noise filter 0, capture task pinned to core 0.
- Analysis ring: 16 slots of 256 samples = 8,192 bytes in PSRAM; one slot stays empty, so it holds at most 15 blocks (240 ms).
- Freshness window: 250 ms (`VoiceBaseInput::fresh`).
- Tracker gap limit: more than 40 ms between the audio timestamps of two blocks.
- Onset strength: high-pass near 40 Hz; bass by two low-pass poles near 180 Hz; four one-pole bands with cutoffs near 200, 800, 2,500 and 6,000 Hz; flux is the mean of the four positive log1p-energy rises; noise floor 40.
- Onsets: threshold mean + 2.2 × mean deviation (smoothing 0.025 and 0.04 per block); refractory time 96 ms; 16 onset timestamps kept.
- Evaluation: history 512 blocks (8.2 s), minimum 384 blocks (6.14 s); every 250 ms of audio time; history decimated by 2 to 32 ms; search 70–190 BPM in 2 BPM steps, then ±2 BPM in 1 BPM steps.
- Double-tempo check: only for winners up to 95 BPM; onset support at least 3 points higher and correlation at least 35 % of the winner's. Onset support counts onset pairs up to 4,000 ms apart that match 1 to 8 beats within max(28 ms, 8 %), weighted 9 minus the number of beats.
- Direct onset tempo: at least 5 onsets; mean of up to 8 recent intervals, each 300–900 ms and within 8 % of the mean; result must lie in 80–180 BPM; match tolerance ±4 BPM to the result, its double or its half.
- Confidence: 0.72 × periodicity + 0.28 × separation; competitors at least 4 BPM from the result; separation = (best − second) / max(0.05, best); floor 55 on direct agreement.
- Adoption: BPM 80–180, confidence at least 30, last onset at most 1,500 ms old (`nowMs - onsetAtMs`, mixing main-loop time and audio time); candidate distance 4 BPM; two evaluations; mean rounded half up, `(candidate + bpm + 1) / 2`.
- Show clock: start tempo 120 BPM (`kDefaultBpm`), 4 beats per bar (`kBeatsPerBar`); beat interval `60000 / BPM` ms, truncated to whole milliseconds. Main loop: one pass about every 5 ms plus work.
- Front button: short press = click; long press = hold of 1,200 ms (debug mode, see the debug-views spec).
