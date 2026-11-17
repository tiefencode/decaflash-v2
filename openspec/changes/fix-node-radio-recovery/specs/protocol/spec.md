# Spec Delta

## MODIFIED Requirements

### Requirement: No automatic return to demo

A Node that has left `Demo` SHALL stay under Mainframe control until it reboots. Radio silence SHALL NOT change its run mode, and the Node SHALL have no holdover state.

#### Scenario: Radio loss while running
- **WHEN** a Node in `MainframeRunning` receives no packets for a long time
- **THEN** it keeps running with the last tempo and scene and does not fall back to `Demo`

#### Scenario: Radio loss while waiting
- **WHEN** a Node in `MainframeWaiting` receives no packets for a long time
- **THEN** it stays in `MainframeWaiting` without rendering beats

## ADDED Requirements

### Requirement: Node radio silence recovery

A Node that has received at least one valid V2 packet since boot SHALL treat a silence period without any valid V2 packet as a possible fault of its own receive path and SHALL repair it. It SHALL repeat the repair after every further silence period until a valid V2 packet arrives. The first repair of a silence SHALL re-initialise ESP-NOW; every later one SHALL restart Wi-Fi and ESP-NOW. The silence period is a timing value, documented under Current values.

#### Scenario: Silence after the Mainframe was heard
- **WHEN** a Node has received a valid V2 packet and then receives none for one silence period
- **THEN** it re-initialises ESP-NOW on channel 1 with the broadcast peer and its receive handler

#### Scenario: Silence continues
- **WHEN** the silence lasts for a second and every further silence period
- **THEN** the Node restarts Wi-Fi in station mode on channel 1 and re-initialises ESP-NOW each time

#### Scenario: Packets return
- **WHEN** a valid V2 packet arrives after one or more repairs
- **THEN** the Node processes it as usual, stops repairing, and the next silence starts again with the ESP-NOW re-initialisation

#### Scenario: Mainframe never heard
- **WHEN** a Node has received no valid V2 packet since boot
- **THEN** it never repairs its radio, however long it runs

#### Scenario: Packet of another installation
- **WHEN** only packets that fail the header validation arrive during the silence period
- **THEN** they do not end the silence

### Requirement: Recovery leaves the show untouched

A radio repair SHALL NOT change the Node's run mode, scene, clock, visual state or text overlay, and SHALL NOT reboot the Node. A failed repair SHALL leave the Node running and SHALL be followed by the next repair after the silence period.

#### Scenario: Repair while running
- **WHEN** a Node in `MainframeRunning` repairs its radio
- **THEN** it stays in `MainframeRunning` and keeps rendering beats with the last tempo and scene

#### Scenario: Repair in demo
- **WHEN** a Node in `Demo` that has heard the Mainframe repairs its radio
- **THEN** it stays in `Demo` and keeps playing its local scene

#### Scenario: Repair step fails
- **WHEN** a step of a repair returns an error
- **THEN** the Node keeps running its output and tries the Wi-Fi restart again after the silence period

### Requirement: Recovery is reported on the serial port

A Node SHALL write one serial line for every repair, naming the length of the silence, the attempt number, the radio channel before the repair, the stage and its result. It SHALL write one line when packets return after a repair, naming the length of the silence and the number of repairs. The `status` command SHALL show whether the radio is ready, the current channel, the age of the last valid packet and the number of repairs since boot.

#### Scenario: Repair logged
- **WHEN** a Node repairs its radio
- **THEN** it prints one line with silence length, attempt number, channel before the repair, stage and result

#### Scenario: Return logged
- **WHEN** the first valid V2 packet arrives after a repair
- **THEN** the Node prints one line with the silence length and the number of repairs that silence needed

#### Scenario: Status
- **WHEN** the user sends `status` over serial
- **THEN** the output contains radio readiness, channel, age of the last valid packet and the repair count
