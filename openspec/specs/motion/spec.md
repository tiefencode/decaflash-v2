# motion Specification

## Purpose

How the Mainframe turns its internal BMI270 IMU into discrete motion events: how the sensor is sampled, how the classifier separates Move, Tap, MultiTap, Impact, Rotate, Reposition and Shake, how often events may be reported, and which parts of the firmware receive them. The classifier is a bounded heuristic that works in g and degrees per second and does no sensor I/O itself. Its knock and hit limits come from a capture in the wooden sphere; the other limits are heuristics or user decisions. What the personality and the message panel do with an event is specified in the personality and message-panel specs. Sources: `apps/mainframe/src/motion_events.{h,cpp}`, the IMU block in `loop()` of `apps/mainframe/src/main.cpp`, and `tests/motion_events.cpp` with `tests/run_motion_events.sh`.

## Requirements

### Requirement: IMU sampling

The Mainframe SHALL enable its internal IMU at boot and, while the IMU is enabled, request a sensor update on every main loop pass. It SHALL pass a sample to the classifier only when that update delivered fresh acceleration and fresh gyroscope data. Each sample SHALL carry three acceleration axes in g, three rotation-rate axes in degrees per second and the loop time in milliseconds.

#### Scenario: Fresh data
- **WHEN** a loop pass finds new acceleration and gyroscope data in the IMU
- **THEN** the six axis values are passed to the classifier with the current loop time

#### Scenario: Only one sensor updated
- **WHEN** an update delivers fresh acceleration but no fresh gyroscope data
- **THEN** no sample is passed in that loop pass

### Requirement: Independent of operating state

The IMU SHALL be sampled and classified in every operating state: during the boot animation, before and after the show starts, with or without audio analysis, on every debug screen and while the personality sleeps. The magnetometer SHALL NOT be an input; tipping and looking direction SHALL be relative quantities from gyroscope and acceleration only.

#### Scenario: Movement during boot
- **WHEN** the Mainframe is moved while the boot animation runs
- **THEN** the movement is classified and can produce a motion event

#### Scenario: Movement while asleep
- **WHEN** the personality sleeps and the device is shaken
- **THEN** a Shake event is produced and handed to the consumers

### Requirement: Start pose from the first calm sample

The classifier SHALL ignore every sample until one is calm, meaning its acceleration magnitude is close to 1 g and its rotation rate is low. That first calm sample SHALL set the start pose and the starting gravity estimate, and SHALL start the looking direction at zero. No event SHALL be produced before it. The start pose SHALL stay fixed until the Mainframe restarts.

#### Scenario: Boot on a table
- **WHEN** the Mainframe boots lying still
- **THEN** the first sample becomes the start pose and classification starts with the next sample

#### Scenario: Boot while being shaken
- **WHEN** every sample after boot is far from 1 g or turning fast
- **THEN** no start pose is set and no event is produced until a calm sample arrives

#### Scenario: Placed differently later
- **WHEN** the device is later set down tipped in a different orientation
- **THEN** the start pose is not updated and tipping is measured against the boot pose

### Requirement: Dynamic acceleration

The classifier SHALL track the gravity vector with a time-based low-pass filter and SHALL treat the difference between the current acceleration vector and that estimate as dynamic acceleration. Window gestures SHALL work on dynamic acceleration and on the rotation-rate magnitude. Tipping SHALL use the raw acceleration vector, so that a strong linear movement does not count as a tip.

#### Scenario: Steady pose
- **WHEN** the device is held still in any orientation for longer than the filter time constant
- **THEN** the dynamic acceleration settles near zero and no gesture window opens

#### Scenario: Short knock
- **WHEN** a knock adds a short acceleration spike
- **THEN** the spike appears as dynamic acceleration because the slow gravity estimate does not follow it

### Requirement: Invalid samples and gaps

A sample with a non-finite value SHALL be discarded, SHALL close any open observation window without an event and SHALL cancel a pending tip hold. When the time since the previous accepted sample exceeds the gap limit, the classifier SHALL do the same and SHALL NOT classify that sample. Missing data SHALL never produce a caught-up gesture.

#### Scenario: Not-a-number value
- **WHEN** a sample contains a NaN axis value during an open window
- **THEN** the window is discarded and no event is reported for it

#### Scenario: Blocked loop
- **WHEN** a knock starts a window and the next sample arrives only after the gap limit
- **THEN** the window is discarded and neither that sample nor the following quiet samples report an event

### Requirement: Observation window

When no window is open, a window SHALL open on the first sample whose dynamic acceleration reaches the burst threshold or whose rotation rate reaches the turn threshold. While open, it SHALL record the peak dynamic acceleration, bursts, move time, active time, strong time, shake reversals and turned angle. It SHALL close on the first sample at or after the window length and yield at most one class, unless it waits for a running burst.

#### Scenario: One window per gesture
- **WHEN** a double knock falls within one window
- **THEN** the window yields one MultiTap, not two Taps

#### Scenario: Gesture across a boundary
- **WHEN** a gesture continues after its window has closed
- **THEN** the rest of the gesture opens a new window only if it still reaches a window threshold, and that window is classified on its own

### Requirement: Waiting for a running burst

At the window length, a window SHALL stay open while a burst is still running, no shake has been detected and either the window is not turning or the running burst has reached the impact level. It SHALL close at the latest at the maximum window length. A burst still running when the window closes SHALL NOT be counted.

#### Scenario: Knock late in the window
- **WHEN** a knock starts shortly before the window length
- **THEN** the window waits until the knock's burst has ended and counts it

#### Scenario: Shake starting late while turning
- **WHEN** a shake starts late in a window in which the sphere is also turning
- **THEN** the window keeps waiting for the strong burst instead of closing on the turning

### Requirement: Bursts

Inside a window a burst SHALL start when dynamic acceleration reaches the burst threshold and end after the burst quiet time below it; its length SHALL run from its first to its last sample at or above the threshold. An ended burst whose peak reaches the impact level SHALL mark a hit if it is no longer than the impact length limit. A weaker ended burst SHALL count as a tap if it is no longer than the knock length limit.

#### Scenario: Normal knock
- **WHEN** a knock produces one short burst below the impact level
- **THEN** one tap is counted

#### Scenario: Hit on the sphere
- **WHEN** a hit produces a burst above the impact level that rings for a few hundred milliseconds within the impact length limit
- **THEN** the window is marked as a hit and no tap is counted for it

#### Scenario: Long strong burst
- **WHEN** a strong burst lasts longer than the impact length limit, as when the sphere rolls or is shaken
- **THEN** it marks neither a hit nor a tap

### Requirement: Shake detection

Inside a window every sample whose dynamic acceleration reaches the shake threshold SHALL be compared with the last stored strong direction. The first such sample SHALL store its direction; a later one SHALL count as a reversal and become the stored direction when its dot product with the stored direction is below the reversal limit. A window SHALL count as a shake only with enough reversals and enough strong time.

#### Scenario: Back-and-forth shaking
- **WHEN** the device is shaken back and forth with strong alternating acceleration for long enough
- **THEN** the window counts as a shake

#### Scenario: Ringing of a hit
- **WHEN** a hit rings and reverses direction several times but stays strong only briefly
- **THEN** the window does not count as a shake

### Requirement: Handling instead of knocking

Inside a window the classifier SHALL integrate the rotation-rate magnitude over time, but only for samples whose rotation rate reaches the turn threshold. A window whose turned angle reaches the handling angle SHALL count as turning; in a turning window bursts SHALL NOT yield Tap or MultiTap. Turning inside a window SHALL NOT by itself report Rotate.

#### Scenario: Picking up the sphere
- **WHEN** the sphere is lifted and turned, with short bumps from the hands
- **THEN** the bumps do not yield a Tap or MultiTap

#### Scenario: Turning without other motion
- **WHEN** the device is turned briskly within a window but neither tips nor completes a full turn
- **THEN** the window reports no Rotate

### Requirement: Moving instead of knocking

A knock leaves the sphere quiet again quickly, while moving it keeps it active. A window SHALL count as busy when its active time exceeds the knock active limit, which grows with the number of counted taps. The taps of a busy window SHALL NOT yield a Tap or MultiTap. A window that opens within the knock calm time after a busy or turning window closed SHALL NOT yield a Tap or MultiTap either, because it is still the same movement.

#### Scenario: Moving the sphere gently by hand
- **WHEN** the sphere is moved so that short bursts occur but the window stays active well beyond the knock active limit
- **THEN** no Tap or MultiTap is produced

#### Scenario: End of a movement
- **WHEN** a window with one short burst opens right after a busy window closed
- **THEN** no Tap is produced

#### Scenario: Knock after the sphere has come to rest
- **WHEN** a knock happens after the knock calm time has passed since the last busy window
- **THEN** it is counted as usual

### Requirement: Window classification

On closing, a window SHALL be classified by the first matching rule: Shake when it counts as a shake; Impact when a hit was marked; MultiTap when two or more taps count as knocks; Tap when exactly one tap counts as a knock and the move time is at most the tap move limit; Move when the move time reaches the Move minimum, with or without turning. Otherwise it SHALL end without an event. Taps count as knocks unless the handling or moving requirements exclude them.

#### Scenario: Shake with high peaks
- **WHEN** a window counts as a shake and also contains a hit
- **THEN** it is classified as Shake

#### Scenario: Lifting with a turn
- **WHEN** the sphere is lifted and turned long enough to reach the Move minimum
- **THEN** the window is classified as Move

#### Scenario: Gentle slide
- **WHEN** a window has no taps, no hit and less move time than the Move minimum
- **THEN** no event is produced

### Requirement: Tipping reports Rotate

The pose angle SHALL be the angle between the current raw acceleration and the start pose. When the acceleration magnitude is close to 1 g, the pose angle has stayed at or beyond the tip angle for the tip hold time and no window is open, the classifier SHALL report Rotate once. It SHALL be armed again only after the pose has come back within the upright angle.

#### Scenario: Tipped backwards and left there
- **WHEN** the device is tipped backwards, forwards or sideways beyond the tip angle and stays there
- **THEN** one Rotate is reported after the tip hold time and no further Rotate follows while it stays tipped

#### Scenario: Set upright and tipped again
- **WHEN** the device is set back within the upright angle and later tipped again
- **THEN** a new Rotate is reported

#### Scenario: Lifting or bumping
- **WHEN** the device is moved so that the acceleration magnitude leaves the band around 1 g
- **THEN** the tip hold restarts and no Rotate is reported from that motion

### Requirement: Rotation around the vertical

The classifier SHALL project the rotation rate onto the gravity estimate to get the rate around the vertical, independent of how the device is mounted. Rates below the dead band SHALL be ignored. Larger rates SHALL be integrated both into the looking direction, kept between −180 and 180 degrees, and into a spin angle. Turning SHALL count as stopped once no rate above the dead band has been seen for the settle time.

#### Scenario: Gyro offset at rest
- **WHEN** the device rests and the gyroscope shows a small offset below the dead band
- **THEN** neither the looking direction nor the spin angle changes

#### Scenario: Mounted on its side
- **WHEN** the device sits on its side in the sphere and the sphere is turned on the table
- **THEN** the turn is measured around the vertical, not around a fixed device axis

### Requirement: Full turn reports Rotate

When the spin angle reaches a full turn, the classifier SHALL report Rotate, reset the spin angle and discard the open window, unless the open window shows a shake reversal or a running burst at the impact level. The spin angle SHALL be reset whenever turning has stopped, so only one full turn in one go counts.

#### Scenario: Carousel
- **WHEN** the sphere is turned a full circle on the table without pausing
- **THEN** one Rotate is reported

#### Scenario: Turn with a pause
- **WHEN** the sphere is turned half a circle, left still for longer than the settle time and then turned another half
- **THEN** no Rotate is reported

### Requirement: Reposition

When the looking direction is at or beyond the reposition angle from the learned direction, turning has stopped and no window is open, the classifier SHALL report Reposition. While the device stays displaced, it SHALL repeat Reposition after each repeat interval. Turning back within the release angle SHALL end the displaced state.

#### Scenario: Turned away and left
- **WHEN** the device is turned beyond the reposition angle around the vertical and left still
- **THEN** Reposition is reported once turning has stopped for the settle time, and repeated every repeat interval

#### Scenario: Turned back
- **WHEN** a displaced device is turned back within the release angle
- **THEN** no further Reposition is reported

### Requirement: Learning a new looking direction

When no turning above the dead band has occurred for the relearn time, the current looking direction SHALL become the learned direction, whatever its offset. A displaced device SHALL therefore stop reporting Reposition at the latest after the relearn time. The start pose used for tipping SHALL NOT be relearned.

#### Scenario: New place on the table
- **WHEN** the sphere is turned to face somewhere else and left there
- **THEN** Reposition repeats until the relearn time has passed and then stops for good

#### Scenario: Turning again later
- **WHEN** after relearning the sphere is turned beyond the reposition angle from its new direction
- **THEN** Reposition is reported again relative to the new direction

### Requirement: One event per sample

A sample SHALL yield at most one event. The classifier SHALL check in this order: a closing window, a full turn, tipping, Reposition. A point event (full turn, tipping, Reposition) that is not reported on a sample SHALL be reported on a later sample if its conditions still hold.

#### Scenario: Window closes during a full turn
- **WHEN** a window closes with a Move on the same sample on which the spin angle reaches a full turn
- **THEN** the Move is reported first and the full turn is checked again on later samples

### Requirement: Global report rate limit

After any reported event the classifier SHALL report no further event of any class until the minimum event spacing has passed. A window classified within that time SHALL be dropped, not delayed. A point event that is due within that time SHALL be reported on the first sample after the spacing if its conditions still hold.

#### Scenario: Continuous shaking
- **WHEN** the device is shaken without pause for several seconds
- **THEN** Shake events are reported no closer together than the minimum event spacing

#### Scenario: Knock right after a Reposition
- **WHEN** a Tap window closes shortly after a Reposition was reported
- **THEN** the Tap is dropped and not reported later

### Requirement: Event record

Every reported event SHALL carry its class, the sample time, a sequence number that grows by one per reported event, a count and a peak in g. For window classes the count SHALL be the number of counted taps in that window and the peak its peak dynamic acceleration; for Rotate and Reposition the count SHALL be 1 and the peak 0. The classifier SHALL keep the last reported event readable without allocation.

#### Scenario: Triple knock
- **WHEN** three knocks in one window are reported as MultiTap
- **THEN** the event has count 3 and the window's peak dynamic acceleration

#### Scenario: No new event
- **WHEN** a sample produces no event
- **THEN** the classifier returns an event of class None and the last reported event stays unchanged

### Requirement: Event consumers

When the classifier reports an event, the main loop SHALL in the same pass first bring the personality up to the current time with the current audio and presence inputs, then hand the event to the personality, then offer it to the message panel. While the mood debug screen is shown, the eye renderer SHALL receive the last reported event. Motion events SHALL NOT be sent over the radio; they reach Nodes only through the mood.

#### Scenario: Tap during a show
- **WHEN** a Tap is reported
- **THEN** the personality is updated to now, applies the Tap as described in the personality spec, and the message panel may show a Tap text as described in the message-panel spec

#### Scenario: Mood debug screen
- **WHEN** the mood debug screen is shown
- **THEN** it can show the class name and count of the last reported event, or `--` with count 0 before the first event, as described in the debug-views spec

### Requirement: Timer wrap-around

All time comparisons in the classifier SHALL use unsigned millisecond differences, so that windows, bursts, holds, gaps, relearning and the rate limit keep working when the millisecond counter wraps around.

#### Scenario: Double knock across the wrap
- **WHEN** a double knock happens while the millisecond counter wraps past its maximum
- **THEN** it is classified as MultiTap as at any other time

## Current values

These are the values in the code on 2026-11-16. They are calibration and may change without a spec change. The knock and hit limits come from `tests/fixtures/motion/` (wooden sphere, 2026-11-16); tipping and Reposition limits are user decisions of 2026-11-16, tried on the device without measurement.

- Sampling: every main loop pass (the loop ends with `delay(5)`); about 70 samples/s on the device, about 35 while a reaction is drawn or played. Sample time is `millis()` at the start of the loop pass.
- Calm sample for the start pose: acceleration magnitude 0.85–1.15 g and rotation rate at most 15 degrees/s.
- Gap limit: more than 150 ms since the previous accepted sample.
- Gravity low-pass: per sample `alpha = dt / (250 ms + dt)`.
- Window: opens at dynamic acceleration ≥0.20 g (burst threshold) or rotation rate ≥45 degrees/s (turn threshold); length 700 ms, at most 1,100 ms while waiting for a burst.
- Burst: threshold 0.20 g, ends after 100 ms below it; knock length limit 130 ms; impact level 0.65 g; impact length limit 500 ms. Tap counter saturates at 255.
- Move time: summed sample intervals at ≥0.30 g. Move minimum 150 ms; tap move limit 120 ms.
- Active time: summed sample intervals at ≥0.10 g. Knock active limit 220 ms for one tap plus 170 ms for each further tap; knock calm time 1000 ms. Measured in the sphere on 2026-11-16: knocks 39–148 ms of active time per window, gentle moving by hand 400–650 ms.
- Shake: strong direction at ≥0.55 g; reversal when the dot product with the stored direction is below −0.15 g²; at least 3 reversals and at least 350 ms at ≥0.45 g (strong time).
- Handling: integrate rotation-rate magnitude while ≥45 degrees/s; handling angle 35 degrees.
- Tipping: acceleration magnitude within 0.12 g of 1 g; tip angle 60 degrees (cosine ≤0.5); upright angle 45 degrees (cosine >0.707); tip hold 300 ms.
- Vertical rotation: dead band 5 degrees/s; settle time 500 ms; full turn 360 degrees.
- Reposition: angle 45 degrees; release angle 30 degrees; repeat interval 9,000 ms, the same as the message panel's cooldown per kind; relearn time 60 s.
- Minimum event spacing: 1,000 ms after any reported event.
- Debug names (`motionName`): `move`, `tap`, `multi-tap`, `impact`, `rotate`, `reposition`, `shake`, and `--` for None.
