# Spec Delta

## ADDED Requirements

### Requirement: Limited effect on behaviour

Sensor samples SHALL reach the personality only through the presence decision of the presence-mood capability, and only as the attention base value. They SHALL NOT change other mood values, eye animation, light directives, sounds or panel texts directly.

#### Scenario: Person in front of the sensor
- **WHEN** a person is considered present
- **THEN** only the attention base value changes; light directives sent to the Nodes and panel texts are the same as without the sensor

#### Scenario: Sensor flags without a present person
- **WHEN** the sensor's presence or motion flag is set but nobody is considered present
- **THEN** mood values, light directives and panel texts are the same as without the sensor

## MODIFIED Requirements

### Requirement: Serial sample line

The Mainframe SHALL print every valid sample as one line over USB serial, for reading live in a serial monitor. The line SHALL start with `TMOS`, followed by space-separated `name=value` fields in this order: `t`, `presence_flag`, `motion_flag`, `shock_flag`, `presence`, `motion`, `shock`, `object`, `object_comp`, `ambient`, `errors`, `person`. `person` is the Mainframe's presence decision after this sample: `away`, `still` or `moving`. Every line names its own fields, so no header line is needed.

#### Scenario: Reading live
- **WHEN** a host reads the Mainframe's USB serial port while the sensor is sampling
- **THEN** it receives one line per sample such as `TMOS t=81234 presence_flag=1 motion_flag=0 shock_flag=0 presence=1940 motion=37 shock=2 object=-1330 object_comp=-1330 ambient=24.1 errors=0 person=still`

#### Scenario: Sensor flag without a present person
- **WHEN** the sensor sets its presence flag but the presence value stays below the Mainframe's entering threshold
- **THEN** the line shows `presence_flag=1` and `person=away`

#### Scenario: No host connected
- **WHEN** no host reads the USB serial port, or the serial output buffer is full
- **THEN** the Mainframe waits at most the short serial transmit timeout (value in design.md), counts a line it could not queue in `errors` and keeps running

#### Scenario: Host starts reading after a full buffer
- **WHEN** lines were dropped because the serial buffer was full and a host then reads the port
- **THEN** `TMOS` lines arrive again at the sample rate without a restart

## REMOVED Requirements

### Requirement: No effect on behaviour

**Reason**: Presence now sets the attention base value; the measurement-only phase is over.
**Migration**: See "Limited effect on behaviour" in this capability and the presence-mood capability.
