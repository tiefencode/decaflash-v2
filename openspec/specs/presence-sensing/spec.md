# presence-sensing Specification

## Purpose
Reads the M5Stack Unit TMOS PIR presence sensor (U185, chip STHS34PF80) on the Mainframe and makes its raw readings available for measurement, without letting the sensor affect the show.

## Requirements

### Requirement: Sensor connection on the Grove port

The Mainframe SHALL look for the presence sensor on the AtomS3R Grove port at I²C address `0x5A` during start-up and, while it is missing, again at a fixed interval. It SHALL apply one fixed sensor configuration (sample rate, averaging; values in the change's design.md) and keep the sensor's own detection thresholds unchanged.

#### Scenario: Sensor present at start-up
- **WHEN** the Mainframe starts with the sensor connected to the Grove port
- **THEN** it identifies the sensor, applies the fixed configuration and starts sampling

#### Scenario: Sensor missing at start-up
- **WHEN** the Mainframe starts without a sensor on the Grove port, or the sensor does not answer with its device identity
- **THEN** it marks the sensor as missing and continues the normal start-up

#### Scenario: Sensor connected later
- **WHEN** the sensor is missing and is then connected to the Grove port while the Mainframe runs
- **THEN** a periodic re-probe finds it, applies the fixed configuration and starts sampling without a restart

### Requirement: Sample contents

Each sample SHALL contain the presence flag, the motion flag, the ambient temperature shock flag, the presence value, the motion value, the ambient shock value, the raw and the compensated object temperature value, the ambient temperature and the time of the sample.

#### Scenario: New data from the sensor
- **WHEN** the sensor reports new data
- **THEN** the Mainframe reads all listed fields from the same sensor update and stores them as the latest sample

### Requirement: Sensor never blocks the show

Reading the sensor SHALL NOT delay audio processing, the show clock or radio sending beyond one short I²C transaction per sample. A missing sensor or a failed read SHALL NOT stop or restart any other part of the Mainframe.

#### Scenario: Read fails while running
- **WHEN** an I²C read of the sensor fails after a successful start
- **THEN** the Mainframe marks the latest sample as invalid, counts the error and keeps running the eye, audio, clock and radio

#### Scenario: Sensor recovers
- **WHEN** reads succeed again after failures
- **THEN** the Mainframe stores new samples as valid again without a restart

### Requirement: Serial sample line

The Mainframe SHALL print every valid sample as one self-describing line over USB serial: `TMOS`, then space-separated `name=value` fields in this order: `t`, `presence_flag`, `motion_flag`, `shock_flag`, `presence`, `motion`, `shock`, `object`, `object_comp`, `ambient`, `errors`, `person` (`away`, `far`, `far_moving` or `near`, the Mainframe's decision after this sample).

#### Scenario: Reading live
- **WHEN** a host reads the Mainframe's USB serial port while the sensor is sampling
- **THEN** it receives one line per sample such as `TMOS t=81234 presence_flag=1 motion_flag=0 shock_flag=0 presence=1940 motion=37 shock=2 object=-1330 object_comp=-1330 ambient=24.1 errors=0 person=near`

#### Scenario: Sensor flag without a present person
- **WHEN** the sensor sets its presence flag but the presence value stays below the Mainframe's entering threshold
- **THEN** the line shows `presence_flag=1` and `person=away`

#### Scenario: No host connected
- **WHEN** no host reads the USB serial port, or the serial output buffer is full
- **THEN** the Mainframe waits at most the short serial transmit timeout (value in design.md), counts a line it could not queue in `errors` and keeps running

#### Scenario: Host starts reading after a full buffer
- **WHEN** lines were dropped because the serial buffer was full and a host then reads the port
- **THEN** `TMOS` lines arrive again at the sample rate without a restart

### Requirement: Meaning of the detection flags

`presence_flag`, `motion_flag` and `shock_flag` SHALL be the sensor's own detection flags, `1` or `0`, unchanged by the Mainframe. `presence_flag` is set while the presence value is above the presence threshold, never by negative values. `motion_flag` is set while the motion value exceeds the motion threshold in either direction. `shock_flag` follows the ambient shock value. Each flag clears after its value falls below its threshold minus the hysteresis.

#### Scenario: Flag follows its value
- **WHEN** the presence value rises above the presence threshold reported in the configuration line
- **THEN** the following lines show `presence_flag=1` until the value falls below the threshold minus the presence hysteresis

#### Scenario: Motion flag after someone leaves
- **WHEN** a person leaves the field of view and the motion value swings strongly negative
- **THEN** the lines show `motion_flag=1` while the presence flag is already off

### Requirement: Meaning of the algorithm values

`presence`, `motion` and `shock` SHALL be the raw signed outputs of the sensor's embedded algorithms, in sensor counts without a physical unit. `presence` is the object signal against a slowly adapting baseline: it rises when a warm body enters the field of view, including one that stands still. `motion` reacts to changes of the object signal and returns towards 0 when the scene is still. `shock` reacts to fast changes of the ambient temperature.

#### Scenario: Person stands still
- **WHEN** a person steps in front of the sensor and then stands still
- **THEN** `motion` rises during the movement and falls back towards 0, while `presence` stays raised as long as the sensor still detects the person

### Requirement: Meaning of the temperature values

`object` SHALL be the raw object signal in sensor counts: the infrared radiation from everything in the field of view relative to the sensor's own temperature, not an absolute temperature. `object_comp` SHALL be the same signal after the sensor's ambient temperature compensation, in the same counts. `ambient` SHALL be the sensor's own temperature in degrees Celsius with one decimal, which follows the surrounding air with a delay.

#### Scenario: Warm body versus warm room
- **WHEN** a person enters the field of view while the room temperature stays the same
- **THEN** `object` rises and `ambient` stays about the same

### Requirement: Meaning of time and error fields

`t` SHALL be the time of the sample in milliseconds since the Mainframe started. `errors` SHALL be the number of failed sensor reads plus dropped serial lines since start.

#### Scenario: Read errors counted
- **WHEN** a sensor read fails and the next read succeeds
- **THEN** the next line shows `errors` increased by at least one

### Requirement: Serial configuration and state lines

After every successful sensor configuration the Mainframe SHALL print one line starting with `TMOS_CONFIG` with the fields `odr_hz`, `avg_tmos`, `avg_t`, `presence_ths`, `presence_hyst`, `motion_ths`, `motion_hyst`, `shock_ths`, `shock_hyst` and `sensitivity` (object signal counts per degree Celsius), read back from the sensor. When the sensor becomes missing or available, it SHALL print one line `TMOS_STATE sensor=missing` or `TMOS_STATE sensor=ok`, with the `errors` field.

#### Scenario: Sensor configured
- **WHEN** the sensor has been found and configured
- **THEN** one `TMOS_CONFIG` line shows the thresholds and hystereses the sensor actually uses

#### Scenario: Cable pulled
- **WHEN** the sensor stops answering while the Mainframe runs
- **THEN** one `TMOS_STATE sensor=missing` line appears and `TMOS` lines stop until the sensor is found again

### Requirement: Limited effect on behaviour

Sensor samples SHALL reach the rest of the Mainframe only through the presence decision and presence events of the presence-mood capability. They SHALL NOT change mood values, sleep, eye animation, light directives, sounds or panel texts in any other way.

#### Scenario: Sensor flags without a present person
- **WHEN** the sensor's presence or motion flag is set but nobody is considered present
- **THEN** mood values, light directives and panel texts are the same as without the sensor

#### Scenario: Light directives
- **WHEN** a person is present
- **THEN** the light directives sent to the Nodes are the same as without the sensor

### Requirement: Serial event line

For every presence event the Mainframe SHALL print one line over USB serial: `TMOS_EVENT`, followed by `t` (milliseconds since start) and `kind`, one of `arrived_in_front`, `walked_past`, `left`, `stands_far` or `moves`. The line SHALL follow the same no-waiting rule as the sample lines.

#### Scenario: Someone walks past
- **WHEN** `Läuft vorbei` is reported at 81234 ms
- **THEN** the serial output contains `TMOS_EVENT t=81234 kind=walked_past`
