# node-device Specification

## Purpose

The device handling of a V2 light Node: which Node kinds and roles exist, how kind and role are stored in non-volatile storage and loaded at boot, how they are switched at runtime, the Atom button gestures, the serial command interface, the boot sequence with its serial output and the RGB filter monitor. Radio packets and the Node run modes are specified in the protocol spec (`openspec/specs/protocol/spec.md`); light rendering and the scenes are specified in the node-output spec. This spec refers to both and does not repeat them. Scene numbers are zero-based indexes, as in the protocol spec. Source: `apps/node/src/main.cpp` (configuration, `switchNodeKind`, `switchNodeRole`, `serviceButton`, `serviceSerial` and its helpers, `setup`, `serviceFilterMonitor`), with `rendererName` from `apps/node/src/node_output.cpp`.

## Requirements

### Requirement: Node kinds and roles

A Node SHALL report device type Node and run as one of two kinds, flashlight or RGB strip. Its role SHALL be one of wash, pulse, accent or flicker. A flashlight SHALL accept only pulse; an RGB strip SHALL accept all four roles. The role none and the UV kind SHALL NOT be usable. The default kind SHALL be flashlight and the default role pulse for both kinds.

#### Scenario: Flashlight with a colour role
- **WHEN** a flashlight Node is asked to take the role wash
- **THEN** the role is not compatible with the kind and the Node keeps pulse

#### Scenario: RGB strip roles
- **WHEN** an RGB strip Node is asked to take wash, pulse, accent or flicker
- **THEN** each of these roles is compatible with the kind

### Requirement: Stored configuration

The Node SHALL keep its kind and role in the Preferences (NVS) namespace `decaflash`: the kind as one byte under key `node_kind` and the role as one byte under key `node_effect`, each holding the raw value of the shared enum (`NodeKind`, `NodeEffect`). At boot the Node SHALL load the kind first and then the role for that kind, and configure its output with both before it prints any boot text.

#### Scenario: Stored RGB accent
- **WHEN** a Node boots with `node_kind` = 1 (RGB strip) and `node_effect` = 3 (accent)
- **THEN** it runs as an RGB strip with the role accent

### Requirement: Fallback for missing or unsupported stored values

If the namespace cannot be opened or a key is missing, the Node SHALL use the default. A stored kind other than flashlight or RGB strip, including UV, SHALL load as flashlight. A stored role that is not wash, pulse, accent or flicker, or that the loaded kind does not accept, SHALL load as the default role. The Node SHALL NOT write a fallback value back to storage at boot.

#### Scenario: First boot
- **WHEN** a Node boots with no `decaflash` namespace in NVS
- **THEN** it runs as a flashlight with the role pulse

#### Scenario: Stored UV kind
- **WHEN** a Node boots with `node_kind` = 2 (UV)
- **THEN** it runs as a flashlight and its role is loaded for a flashlight

#### Scenario: Stored role does not fit the kind
- **WHEN** a Node boots with `node_kind` = 0 (flashlight) and `node_effect` = 1 (wash)
- **THEN** it runs as a flashlight with the role pulse, and NVS still holds wash

### Requirement: Storing a change

Every kind or role change made by button or serial command SHALL be stored. A kind change SHALL store the new kind and then the role the Node ends up with; a role change SHALL store the role. If a write fails, the Node SHALL print `CONFIG: save_failed` and still apply the change at runtime.

#### Scenario: Kind change stores the effective role
- **WHEN** an RGB strip Node with the role wash switches to flashlight
- **THEN** NVS holds kind flashlight and role pulse

#### Scenario: Write fails
- **WHEN** writing the new role to NVS fails
- **THEN** the Node prints `CONFIG: save_failed` and runs with the new role until it reboots

### Requirement: Kind switch

On a kind switch the Node SHALL keep its current role if the new kind accepts it, otherwise take the default role. It SHALL clear any text overlay, reconfigure its output, print its profile and scene list, and apply the scene as in "Applying a new profile". A switch to the kind the Node already has SHALL run the same way.

#### Scenario: RGB strip with pulse to flashlight
- **WHEN** an RGB strip Node with the role pulse switches to flashlight
- **THEN** it keeps the role pulse

#### Scenario: Same kind again
- **WHEN** a flashlight Node in `Demo` receives `mode flash`
- **THEN** it stores the kind again, prints its profile and restarts with scene 0

### Requirement: Role switch

A role switch to a role the current kind does not accept SHALL change nothing, store nothing and print `SERIAL: role_mismatch role=<role> mode=<kind>`. A compatible role SHALL be stored; the Node SHALL then clear any text overlay, reconfigure its output, print its profile and scene list and apply the scene as in "Applying a new profile".

#### Scenario: Wash on a flashlight
- **WHEN** a flashlight Node receives `role wash`
- **THEN** it prints `SERIAL: role_mismatch role=wash mode=flash` and keeps its role and output

#### Scenario: Flicker on an RGB strip
- **WHEN** an RGB strip Node receives `role flicker`
- **THEN** it stores flicker and switches its output to the flicker role

### Requirement: Applying a new profile

After a kind or role switch, a Node in `Demo` SHALL restart its demo clock and select scene 0 with an announcement. A Node in `MainframeWaiting` or `MainframeRunning` SHALL keep its run mode, clock and scene index and switch its output to the command that scene defines for the new kind and role. Applying the command SHALL end a mute.

#### Scenario: Role change during the show
- **WHEN** an RGB strip Node in `MainframeRunning` on scene 1 changes its role
- **THEN** it stays in `MainframeRunning` on scene 1 and plays that scene's command for the new role

#### Scenario: Role change in demo
- **WHEN** an RGB strip Node in `Demo` on scene 1 changes its role
- **THEN** it restarts the demo clock and plays scene 0 for the new role

### Requirement: Role cycle

For a role cycle an RGB strip SHALL take the next role in the order wash, pulse, accent, flicker and then wash again; from any other role it SHALL take wash. A flashlight SHALL always cycle to pulse.

#### Scenario: Cycle from flicker
- **WHEN** an RGB strip Node with the role flicker cycles its role
- **THEN** its new role is wash

### Requirement: Button debounce

The Node SHALL read the Atom front button as active low. A level change SHALL count only after the level has stayed unchanged for the debounce time. A debounced press SHALL start one gesture, which ends at the debounced release. Debounce time and pin are under Current values.

#### Scenario: Contact bounce
- **WHEN** the button level toggles several times within the debounce time and then stays low
- **THEN** the Node registers one press once the level has stayed low for the debounce time

### Requirement: Short press

A debounced release without a long press during the gesture SHALL count as a short press. In `Demo` the Node SHALL print `BUTTON: short_press` and select the next local scene, wrapping after the last; this also ends a mute. In `MainframeWaiting` and `MainframeRunning` a short press SHALL do nothing and print nothing.

#### Scenario: Short press in demo
- **WHEN** a Node in `Demo` on its last scene gets a short press
- **THEN** it prints `BUTTON: short_press` and plays scene 0 with an announcement

#### Scenario: Short press under Mainframe control
- **WHEN** a Node in `MainframeRunning` gets a short press
- **THEN** its scene, output and log stay unchanged

### Requirement: Long press per Node kind

When the button has been held for the long-press time, the Node SHALL act once for that gesture, while the button is still held, and print `BUTTON: long_press`, in every run mode. A flashlight SHALL mute its output. An RGB strip SHALL cycle to its next role as in "Role cycle" and store it. The release after a long press SHALL NOT count as a short press.

#### Scenario: Long press on a flashlight
- **WHEN** a flashlight Node's button is held past the long-press time
- **THEN** the Node prints `BUTTON: long_press` and turns its output off before the button is released

#### Scenario: Long press on an RGB strip under Mainframe control
- **WHEN** an RGB strip Node in `MainframeRunning` with the role pulse gets a long press
- **THEN** it stores and plays the role accent and stays in `MainframeRunning`

### Requirement: Output mute

A mute SHALL clear any text overlay, stop a running flash burst and turn the output off. While muted the Node SHALL keep its clock and run mode but render nothing on beats and stop servicing its output. A mute SHALL end only when a new scene command is applied: a profile switch, a short press in `Demo`, a hello, or a scene packet whose command differs from the active one. A further long press SHALL NOT end it.

#### Scenario: Unmute in demo
- **WHEN** a muted flashlight Node in `Demo` gets a short press
- **THEN** it selects the next scene and its output runs again

#### Scenario: Same scene repeated while muted
- **WHEN** a muted flashlight Node in `MainframeRunning` receives the periodic scene packet for its current scene
- **THEN** it stays muted

#### Scenario: Second long press
- **WHEN** a muted flashlight Node gets another long press
- **THEN** it stays muted

### Requirement: Serial line input

The Node SHALL read serial commands as lines ended by a line feed, ignoring carriage returns. A line SHALL hold at most the line capacity; further characters SHALL be dropped until the line ends. Before matching, the Node SHALL trim leading and trailing white space and convert the line to lower case. An empty line SHALL be ignored.

#### Scenario: Mixed case with spaces
- **WHEN** the line `  Mode RGB\r\n` arrives
- **THEN** the Node processes it as `mode rgb`

#### Scenario: Unknown command
- **WHEN** the line `blink` arrives
- **THEN** the Node prints `SERIAL: unknown_command 'blink'` and `SERIAL: use 'help'`

### Requirement: Serial commands

The Node SHALL accept exactly these commands: `help` prints the help text, `status` prints the status report, `mode rgb` or `mode rgbstrip` switches to RGB strip, `mode flash` or `mode flashlight` switches to flashlight, and the role commands of "Serial role names". Mode and role commands SHALL switch and store as in "Kind switch" and "Role switch".

#### Scenario: Switch to RGB strip
- **WHEN** a flashlight Node receives `mode rgbstrip`
- **THEN** it switches to RGB strip, stores the kind and prints its new profile

### Requirement: Serial role names

`role wash`, `role pulse`, `role accent` and `role flicker` SHALL select the named role, and `effect` SHALL work in place of `role` for these four names. `effect` SHALL also accept legacy names: `ambient` and `bluewash` for wash, `bluepulse` and `flashpulse` for pulse, `redaccent` and `flashaccent` for accent, `motion` and `blueredflicker` for flicker. `role` SHALL NOT accept the legacy names.

#### Scenario: Legacy effect name
- **WHEN** an RGB strip Node receives `effect redaccent`
- **THEN** it switches to the role accent

#### Scenario: Legacy name with role
- **WHEN** a Node receives `role redaccent`
- **THEN** it prints `SERIAL: unknown_command 'role redaccent'`

### Requirement: Help text

The help text SHALL list the button gestures for the current kind and the serial commands. A flashlight SHALL show short press as next scene and long press as turning the output off, and `role pulse` as its only role; an RGB strip SHALL show short press as next scene in demo only, long press as next role, and all four roles. Both SHALL list `mode rgb`, `mode flash`, the `effect` alias, `status` and `help`.

#### Scenario: Help on an RGB strip
- **WHEN** an RGB strip Node receives `help`
- **THEN** it prints `long press  -> next role` and `role wash | pulse | accent | flicker`

### Requirement: Status report

`status` SHALL print, between `-----` lines, the kind (`NODE: mode=`), the role (`NODE: role=`), the renderer name, the scene number, count and name with the active command names, the run mode (`demo`, `mainframe_wait` or `mainframe`), the BPM, the mute flag and the clock's bar and beat counters. An RGB strip SHALL add the filter monitor lines when its renderer reports an active modulation.

#### Scenario: Status of a muted flashlight
- **WHEN** a muted flashlight Node in `Demo` receives `status`
- **THEN** the report contains `NODE: mode=flash`, `RENDERER: flash` and `CONTROL: mode=demo bpm=120 muted=1`

### Requirement: Boot sequence

At boot the Node SHALL start the serial port, wait a short start-up delay, configure the button input, load and apply its stored kind and role, print its identity, start its radio as the protocol spec defines, print the radio results, the scene list and the help text, and then enter `Demo` with scene 0 and an announcement. It SHALL schedule the first filter monitor report one monitor interval after boot.

#### Scenario: Boot without radio
- **WHEN** a Node boots and the radio start-up fails
- **THEN** it prints `ESP-NOW: state=failed` and still enters `Demo` with scene 0

### Requirement: Boot output

Before radio start the Node SHALL print `Decaflash Node V2`, a `DEVICE:` line with the numeric device type, kind and role, the kind and role names, a `RUNTIME:` label for the kind, `CONTROLS: atom button + serial`, the renderer name, on an RGB strip the LED driver and pins, and the demo clock. After radio start it SHALL print each radio step result and `ESP-NOW: state=ok` or `failed`.

#### Scenario: Boot of an RGB strip with the role wash
- **WHEN** an RGB strip Node with the role wash boots
- **THEN** it prints `DEVICE: type=1 node_kind=1 node_role=1`, `RUNTIME: standalone rgb demo`, `RENDERER: rgb_strip` and `RGB-DRIVER: sk6812 pins=26+32`

#### Scenario: Boot of a flashlight
- **WHEN** a flashlight Node boots
- **THEN** it prints `RUNTIME: standalone flash demo` and `RENDERER: flash` and no `RGB-DRIVER:` line

### Requirement: Filter monitor

An unmuted RGB strip Node SHALL print the state of its time-based surface modulation once per monitor interval, in every run mode, whenever the renderer reports it active: `ACTIVITY: <n>`, `NOISE: shadow=<n> pocket=<n> cool=<n>` and `COLOR: <side> <strength>`. Side SHALL be blue, neutral or magenta by colour drift; strength SHALL be twice the drift's distance from 127. A flashlight SHALL print no monitor lines.

#### Scenario: Magenta drift
- **WHEN** the monitor interval has passed on an RGB strip Node and the colour drift is 200
- **THEN** the Node prints `COLOR: magenta 146` after the activity and noise lines

#### Scenario: RGB command off
- **WHEN** the monitor interval passes while the RGB strip plays a command with pattern off
- **THEN** the renderer reports no active modulation and the Node prints no monitor lines

## Current values

These are the values in the code on 2026-11-15. They may change without a spec change.

- Button: Atom front button on GPIO 39, input without internal pull-up, active low (`BUTTON_PIN`).
- Debounce time: 30 ms (`BUTTON_DEBOUNCE_MS`); long-press time: 900 ms (`BUTTON_LONG_PRESS_MS`).
- Start-up delay after `Serial.begin`: 500 ms (`STARTUP_DELAY_MS`); serial speed 115200 baud.
- Serial line capacity: 48 bytes including the terminator, so 47 characters (`kSerialLineCapacity`).
- Filter monitor interval: 5 s (`FILTER_MONITOR_INTERVAL_MS`).
- Colour drift sides: below 96 blue, above 160 magenta, otherwise neutral.
- Demo clock printed at boot: `CLOCK: 120 bpm 4/4` (`BPM`, `DEFAULT_BEATS_PER_BAR`).
- RGB LED driver: SK6812 on GPIO 26 and 32.
- Enum values: device type Node = 1; kind flashlight = 0, RGB strip = 1, UV = 2; role none = 0, wash = 1, pulse = 2, accent = 3, flicker = 4 (`shared/include/decaflash_types.h`).
- Names in serial output: kinds `flash`, `rgb`; renderers `flash`, `rgb_strip`; run modes `demo`, `mainframe_wait`, `mainframe`.
