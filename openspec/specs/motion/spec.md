# motion Specification

## Purpose

How the Mainframe turns its internal BMI270 IMU into discrete motion events: how the sensor is sampled, how the classifier separates Move, Tap, MultiTap, Impact, Rotate, Tilt and Shake, how often events may be reported, and which parts of the firmware receive them. The classifier is a bounded heuristic that works in g and degrees per second; it does no sensor I/O itself and is not gesture recognition validated on the enclosure. What the personality and the message panel do with an event is specified in the personality and message-panel specs. Sources: `apps/mainframe/src/motion_events.{h,cpp}`, `apps/mainframe/src/main.cpp` (`setup()` IMU configuration, IMU block in `loop()`), `apps/mainframe/src/eye_renderer.cpp` (debug overlay line) and `tests/motion_events.cpp` with `tests/run_motion_events.sh`.

## Requirements

### Requirement: IMU sampling

The Mainframe SHALL enable its internal IMU at boot and, while the IMU is enabled, request a sensor update from the main loop at most once per sampling interval. It SHALL pass a sample to the classifier only when that update delivered fresh acceleration and fresh gyroscope data; otherwise the attempt SHALL be skipped until the next interval. Each sample SHALL carry three acceleration axes in g, three rotation-rate axes in degrees per second and the loop time in milliseconds.

#### Scenario: Fresh data
- **WHEN** a sampling interval has passed and the IMU reports new acceleration and gyroscope data
- **THEN** the six axis values are passed to the classifier with the current loop time

#### Scenario: Only one sensor updated
- **WHEN** an update delivers fresh acceleration but no fresh gyroscope data
- **THEN** no sample is passed and the next attempt waits for the next sampling interval

### Requirement: Independent of operating state

The IMU SHALL be sampled and classified in every operating state: during the boot animation, before and after the show starts, with or without audio analysis, and while the personality sleeps. The magnetometer SHALL NOT be an input; rotation and tilt SHALL be relative quantities from gyroscope and acceleration only.

#### Scenario: Movement during boot
- **WHEN** the Mainframe is moved while the boot animation runs
- **THEN** the movement is classified and can produce a motion event

#### Scenario: Movement while asleep
- **WHEN** the personality sleeps and the device is shaken
- **THEN** a Shake event is produced and handed to the consumers

### Requirement: Reference pose from the first calm sample

The classifier SHALL ignore every sample until one is calm, meaning its acceleration magnitude is close to 1 g and its rotation rate is low. That first calm sample SHALL set both the reference pose and the starting gravity estimate. No event SHALL be produced before it. The reference pose SHALL stay fixed until the Mainframe restarts.

#### Scenario: Boot on a table
- **WHEN** the Mainframe boots lying still
- **THEN** the first sample becomes the reference pose and classification starts with the next sample

#### Scenario: Boot while being shaken
- **WHEN** every sample after boot is far from 1 g or turning fast
- **THEN** no reference pose is set and no event is produced until a calm sample arrives

#### Scenario: Placed differently later
- **WHEN** the device is later set down in a different orientation
- **THEN** the reference pose is not updated and the new orientation is measured against the boot pose

### Requirement: Dynamic acceleration

The classifier SHALL track the gravity vector with a time-based low-pass filter and SHALL treat the difference between the current acceleration vector and that estimate as dynamic acceleration. Gesture classes other than Tilt SHALL work on dynamic acceleration vectors and on the rotation-rate magnitude, not on raw acceleration magnitude.

#### Scenario: Steady pose
- **WHEN** the device is held still in any orientation for longer than the filter time constant
- **THEN** the dynamic acceleration settles near zero and no gesture window opens

#### Scenario: Short knock
- **WHEN** a knock adds a short acceleration spike
- **THEN** the spike appears as dynamic acceleration because the slow gravity estimate does not follow it

### Requirement: Invalid samples and gaps

A sample with a non-finite value SHALL be discarded, SHALL close any open observation window without an event and SHALL cancel a pending tilt hold. When the time since the previous accepted sample exceeds the gap limit, the classifier SHALL do the same and SHALL NOT classify that sample. Missing data SHALL never produce a caught-up gesture.

#### Scenario: Not-a-number value
- **WHEN** a sample contains a NaN axis value during an open window
- **THEN** the window is discarded and no event is reported for it

#### Scenario: Blocked loop
- **WHEN** a knock starts a window and the next sample arrives only after the gap limit
- **THEN** the window is discarded and neither that sample nor the following quiet samples report an event

### Requirement: Observation window

When no window is open, a window SHALL open on the first sample whose dynamic acceleration reaches the move threshold or whose rotation rate reaches the turn threshold. While open, it SHALL record the peak dynamic acceleration, the time spent at or above the move threshold, short impulses, shake reversals and turned angle. On the first sample at or after the window length it SHALL close and yield at most one class.

#### Scenario: One window per gesture
- **WHEN** a double knock falls within one window
- **THEN** the window closes after the window length and yields one MultiTap, not two Taps

#### Scenario: Gesture across a boundary
- **WHEN** a gesture continues after its window has closed
- **THEN** the rest of the gesture opens a new window only if it still reaches a window threshold, and that window is classified on its own

### Requirement: Short impulse counting

Inside a window an impulse SHALL start when dynamic acceleration reaches the knock threshold and end when it falls below the release threshold. An ended impulse SHALL count as a tap only if it lasted at most the maximum tap duration and, unless it is the first in the window, started at least the minimum tap spacing after the previous counted one. An impulse still open when the window closes SHALL NOT count.

#### Scenario: Lifting the device
- **WHEN** the device is lifted with a sustained acceleration that stays above the release threshold longer than the maximum tap duration
- **THEN** no tap is counted

#### Scenario: Ringing after a knock
- **WHEN** one knock rings and crosses the knock threshold again within the minimum tap spacing
- **THEN** only one tap is counted

### Requirement: Shake reversals

Inside a window every sample whose dynamic acceleration reaches the shake threshold SHALL be compared with the last stored strong direction. The first such sample SHALL store its direction; a later one SHALL count as a reversal and become the stored direction only when its dot product with the stored direction is below the reversal limit. A high level without direction changes SHALL NOT count.

#### Scenario: Back-and-forth shaking
- **WHEN** the device is shaken back and forth with strong alternating acceleration
- **THEN** each change of direction counts as a reversal

#### Scenario: One hard push
- **WHEN** a single strong push points in one direction only
- **THEN** no reversal is counted

### Requirement: Turned angle

Inside a window the classifier SHALL integrate the rotation-rate magnitude over time, but only for samples whose rotation rate reaches the turn threshold. Turning around any axis SHALL count the same, so forward pitch, sideways roll and turning flat all add to the angle.

#### Scenario: Pitch forward
- **WHEN** the device is pitched forward fast enough and far enough within one window
- **THEN** the turned angle reaches the Rotate angle

#### Scenario: Short gyro spike
- **WHEN** a linear movement carries a brief rotation spike
- **THEN** the integrated angle stays below the Rotate angle and no Rotate is classified

### Requirement: Window classification

On closing, a window SHALL be classified by the first matching rule: Shake when the reversals reach the shake count; Impact when the peak reaches the impact level; Rotate when the turned angle reaches the Rotate angle; MultiTap when two or more taps were counted; Tap when exactly one tap was counted and the move time is at most the tap move limit; Move when the move time reaches the Move minimum. Otherwise the window SHALL end without an event.

#### Scenario: Shake with high peaks
- **WHEN** a window has enough reversals and also a peak above the impact level
- **THEN** it is classified as Shake

#### Scenario: Single knock
- **WHEN** a window contains one counted tap and little other motion
- **THEN** it is classified as Tap

#### Scenario: Gentle slide
- **WHEN** a window has no taps and less move time than the Move minimum
- **THEN** no event is produced

### Requirement: Tilt detection

The pose SHALL be the angle between the current raw acceleration and the reference pose. Tilt SHALL be reported when no window is open, the pose has stayed calm and beyond the tilt angle for the tilt hold time, and either the device was not tilted before or the tilt repeat interval has passed since the last Tilt. A pose back within the release angle SHALL clear the tilted state.

#### Scenario: Held tilt
- **WHEN** the device is tilted beyond the tilt angle and then held still
- **THEN** a Tilt is reported after the hold time and repeated every repeat interval while it stays tilted

#### Scenario: Return upright
- **WHEN** the device is turned back within the release angle and tilted again later
- **THEN** the next Tilt is reported after a new hold time without waiting for the repeat interval

#### Scenario: Lifting and knocking
- **WHEN** the device is moved or knocked so that the acceleration magnitude leaves the calm band or it turns fast
- **THEN** the tilt hold restarts and no Tilt is reported from that motion

### Requirement: Global report rate limit

After any reported event the classifier SHALL report no further event of any class, Tilt included, until the minimum event spacing has passed. A window classified within that time SHALL be dropped, not delayed. A Tilt that is due within that time SHALL be reported on the first sample after the spacing if its conditions still hold.

#### Scenario: Continuous shaking
- **WHEN** the device is shaken without pause for several seconds
- **THEN** Shake events are reported no closer together than the minimum event spacing

#### Scenario: Knock right after an event
- **WHEN** a Tap window closes shortly after a Rotate was reported
- **THEN** the Tap is dropped and not reported later

### Requirement: Event record

Every reported event SHALL carry its class, the sample time, a sequence number that grows by one per reported event, a count and a peak in g. For window classes the count SHALL be the number of counted taps in that window and the peak its peak dynamic acceleration; for Tilt the count SHALL be 1 and the peak 0. The classifier SHALL keep the last reported event readable without allocation.

#### Scenario: Triple knock
- **WHEN** three knocks in one window are reported as MultiTap
- **THEN** the event has count 3 and the window's peak dynamic acceleration

#### Scenario: No new event
- **WHEN** a sample produces no event
- **THEN** the classifier returns an event of class None and the last reported event stays unchanged

### Requirement: Event consumers

When the classifier reports an event, the main loop SHALL in the same pass first bring the personality up to the current time with the current audio inputs, then hand the event to the personality, then offer it to the message panel. While the mood debug overlay is on, the eye renderer SHALL receive the last reported event so the overlay can show its class name and count. Motion events SHALL NOT be sent over the radio; they reach Nodes only through the mood.

#### Scenario: Tap during a show
- **WHEN** a Tap is reported
- **THEN** the personality is updated to now, applies the Tap as described in the personality spec, and the message panel may show a Tap text as described in the message-panel spec

#### Scenario: Debug overlay
- **WHEN** the mood debug overlay is on
- **THEN** it shows the class name and count of the last reported event, or `--` with count 0 before the first event

### Requirement: Timer wrap-around

All time comparisons in the classifier SHALL use unsigned millisecond differences, so that windows, holds, gaps and the rate limit keep working when the millisecond counter wraps around.

#### Scenario: Double knock across the wrap
- **WHEN** a double knock happens while the millisecond counter wraps past its maximum
- **THEN** it is classified as MultiTap as at any other time

## Current values

These are the values in the code on 2026-11-15. They are calibration and may change without a spec change.

- Sampling interval: 20 ms between update attempts (`lastImuAtMs` in `main.cpp`); the main loop adds `delay(5)` per pass. Sample time is `millis()` at the start of the loop pass, not a sensor timestamp.
- Calm sample for the reference pose: acceleration magnitude 0.85–1.15 g and rotation rate at most 15 degrees/s.
- Gap limit: more than 150 ms since the previous accepted sample.
- Gravity low-pass: per sample `alpha = dt / (250 ms + dt)`.
- Window opening: dynamic acceleration ≥0.30 g (move threshold) or rotation rate ≥45 degrees/s (turn threshold). Window length: 700 ms from the opening sample.
- Move time: summed sample intervals with dynamic acceleration ≥0.30 g. Move minimum: 150 ms. Tap move limit: 120 ms.
- Impulse: starts at ≥0.45 g (knock threshold), ends below 0.20 g (release threshold); maximum tap duration 120 ms; minimum tap spacing 100 ms between impulse starts. Tap counter saturates at 255.
- Shake: strong direction at dynamic acceleration ≥0.55 g; reversal when the dot product with the stored direction is below −0.15 g²; shake count 3 reversals. Reversal counter saturates at 255.
- Rotate: integrate rotation-rate magnitude while ≥45 degrees/s; Rotate angle 35 degrees.
- Impact level: peak dynamic acceleration ≥1.8 g.
- Tilt: calm pose means acceleration magnitude within 0.12 g of 1 g and rotation rate below 15 degrees/s; tilt angle more than about 25 degrees (cosine below 0.906); release angle within about 15 degrees (cosine above 0.966); hold time 500 ms; repeat interval 2,000 ms.
- Minimum event spacing: 1,000 ms after any reported event.
- Debug names (`motionName`): `move`, `tap`, `multi-tap`, `impact`, `rotate`, `tilt`, `shake`, and `--` for None.
