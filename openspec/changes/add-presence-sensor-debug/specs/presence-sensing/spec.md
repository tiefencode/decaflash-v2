# Spec Delta

## Purpose

Reads the M5Stack Unit TMOS PIR presence sensor (U185, chip STHS34PF80) on the Mainframe and makes its raw readings available for measurement, without letting the sensor affect the show.

## ADDED Requirements

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

### Requirement: No effect on behaviour

Sensor samples SHALL NOT change mood values, eye animation, light directives, sounds or panel texts. They SHALL be used only for the presence debug screen and the serial sample output.

#### Scenario: Person in front of the sensor
- **WHEN** the presence or motion flag is set
- **THEN** mood values, light directives sent to the Nodes and panel texts are the same as without the sensor

### Requirement: Serial sample output

The Mainframe SHALL print every valid sample as one CSV line over USB serial. Each line SHALL start with the fixed tag `TMOS,` followed by the sample time in milliseconds and all sample fields in a fixed order. It SHALL repeat a header line tagged `TMOS_HEADER,` that names the columns at start-up and at a fixed interval, so a host that connects later still learns the column order.

#### Scenario: Recording a measurement
- **WHEN** a host reads the Mainframe's USB serial port while the sensor is sampling
- **THEN** it receives one `TMOS,` line per sample whose columns match the last `TMOS_HEADER,` line

#### Scenario: No host connected
- **WHEN** no host reads the USB serial port
- **THEN** the Mainframe keeps running without waiting for the serial output
