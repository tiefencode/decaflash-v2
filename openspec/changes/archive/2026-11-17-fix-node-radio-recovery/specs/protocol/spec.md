# Spec Delta

## MODIFIED Requirements

### Requirement: No automatic return to demo

A Node that has left `Demo` SHALL stay under Mainframe control until it reboots. Radio silence shorter than the reboot period SHALL NOT change its run mode, and the Node SHALL have no holdover state. The only automatic way back to `Demo` SHALL be the reboot after long silence defined in this spec.

#### Scenario: Radio loss while running
- **WHEN** a Node in `MainframeRunning` receives no packets for less than the reboot period
- **THEN** it keeps running with the last tempo and scene and does not fall back to `Demo`

#### Scenario: Radio loss while waiting
- **WHEN** a Node in `MainframeWaiting` receives no packets for less than the reboot period
- **THEN** it stays in `MainframeWaiting` without rendering beats

## ADDED Requirements

### Requirement: Node radio silence recovery

A Node SHALL treat every silence period without a valid V2 packet, counted from boot or from the last valid V2 packet, as a possible fault of its own receive path and SHALL repair it. It SHALL repeat the repair after every further silence period until a valid V2 packet arrives. The first repair of a silence SHALL re-initialise ESP-NOW; every later one SHALL restart the Wi-Fi driver and ESP-NOW. The silence period is a timing value, documented under Current values.

#### Scenario: First silence period
- **WHEN** a Node receives no valid V2 packet for one silence period
- **THEN** it re-initialises ESP-NOW on channel 1 with the broadcast peer and its receive handler

#### Scenario: Silence continues
- **WHEN** the silence lasts for a second and every further silence period
- **THEN** the Node restarts the Wi-Fi driver in station mode on channel 1 and re-initialises ESP-NOW each time

#### Scenario: Packets return
- **WHEN** a valid V2 packet arrives after one or more repairs
- **THEN** the Node processes it as usual, stops repairing, and the next silence starts again with the ESP-NOW re-initialisation

#### Scenario: Mainframe never heard
- **WHEN** a Node has received no valid V2 packet since boot
- **THEN** it repairs its radio after every silence period all the same

#### Scenario: Radio start-up failed
- **WHEN** the radio start-up of a Node failed at boot
- **THEN** the repair after the first silence period tries to bring the radio up again

#### Scenario: Packet of another installation
- **WHEN** only packets that fail the header validation arrive during the silence period
- **THEN** they do not end the silence

### Requirement: Repair leaves the show untouched

A radio repair SHALL NOT change the Node's run mode, scene, clock, visual state or text overlay, and SHALL NOT reboot the Node. A failed repair SHALL leave the Node running and SHALL be followed by the next repair after the silence period.

#### Scenario: Repair while running
- **WHEN** a Node in `MainframeRunning` repairs its radio
- **THEN** it stays in `MainframeRunning` and keeps rendering beats with the last tempo and scene

#### Scenario: Repair in demo
- **WHEN** a Node in `Demo` repairs its radio
- **THEN** it stays in `Demo` and keeps playing its local scene

#### Scenario: Repair step fails
- **WHEN** a step of a repair returns an error
- **THEN** the Node keeps running its output and tries the Wi-Fi driver restart after the next silence period

### Requirement: Node reboot after long silence

A Node in `MainframeWaiting` or `MainframeRunning` that has received no valid V2 packet for the reboot period SHALL reboot itself. A Node in `Demo` SHALL NOT reboot because of silence. Directly before rebooting the Node SHALL check once more for a newly arrived valid V2 packet and SHALL NOT reboot if there is one. It SHALL switch its outputs off before rebooting. The reboot period is a timing value, documented under Current values.

#### Scenario: Long silence under Mainframe control
- **WHEN** a Node in `MainframeRunning` or `MainframeWaiting` has received no valid V2 packet for the reboot period
- **THEN** it switches its outputs off, reboots and comes up in `Demo` with its stored Node kind and role

#### Scenario: No reboot loop
- **WHEN** a Node has rebooted because of silence and still receives no packets
- **THEN** it stays in `Demo`, keeps repairing its radio and does not reboot again

#### Scenario: Long silence in demo
- **WHEN** a Node in `Demo` receives no valid V2 packet for longer than the reboot period
- **THEN** it does not reboot

#### Scenario: Packet arrives at the last moment
- **WHEN** a valid V2 packet arrives after the reboot period has elapsed but before the Node reboots
- **THEN** the Node does not reboot and the silence ends

#### Scenario: Single packet during the silence
- **WHEN** one valid V2 packet arrives before the reboot period has elapsed
- **THEN** the reboot period starts again from that packet

### Requirement: Recovery is reported on the serial port

A Node SHALL write one serial line for every repair, naming the length of the silence, the attempt number, the radio channel before the repair, the stage and its result. It SHALL write one line when packets return after a repair and one line before a reboot because of silence. At boot it SHALL print the reason of the last reset. The `status` command SHALL show whether the radio is ready, the current channel, the age of the last valid packet and the number of repairs since boot.

#### Scenario: Repair logged
- **WHEN** a Node repairs its radio
- **THEN** it prints one line with silence length, attempt number, channel before the repair, stage and result

#### Scenario: Return logged
- **WHEN** the first valid V2 packet arrives after a repair
- **THEN** the Node prints one line with the silence length and the number of repairs that silence needed

#### Scenario: Reboot logged
- **WHEN** a Node reboots because of silence
- **THEN** it prints one line with the silence length before rebooting, and the reset reason after it came up

#### Scenario: Status
- **WHEN** the user sends `status` over serial
- **THEN** the output contains radio readiness, channel, age of the last valid packet and the repair count
