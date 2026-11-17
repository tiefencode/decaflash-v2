# protocol Specification

## Purpose

The radio protocol between the Mainframe and the light Nodes of one Decaflash V2 installation: packet header and identity, the five message types with their layouts, when the Mainframe sends them, how a Node reacts to them, and the Node run modes they drive. It also fixes what the protocol deliberately does not do. Sources: `shared/include/protocol.h`, `shared/include/espnow_transport.h`, `shared/include/decaflash_types.h`, `apps/mainframe/src/main.cpp`, `apps/node/src/main.cpp` and `tests/protocol_identity.cpp`.

## Requirements

### Requirement: Broadcast transport on channel 1

Both device types SHALL use ESP-NOW in Wi-Fi station mode on Wi-Fi channel 1, with one unencrypted broadcast peer (`FF:FF:FF:FF:FF:FF`) whose peer channel is 0, meaning the current radio channel. Every protocol packet SHALL be sent to the broadcast address.

#### Scenario: Radio start-up
- **WHEN** the Mainframe or a Node initialises its radio
- **THEN** it sets Wi-Fi station mode, sets channel 1, initialises ESP-NOW and adds the unencrypted broadcast peer
- **AND** it does not associate with an access point

#### Scenario: Radio start-up fails
- **WHEN** any step of the radio start-up or the broadcast peer setup fails
- **THEN** the device treats its radio as not ready and keeps running without radio
- **AND** a Mainframe without a ready radio sends no packets, and a Node without a ready radio registers no receive handler

### Requirement: Packet header

Every packet SHALL start with an 8-byte header: a 32-bit magic value, a 16-bit payload version, an 8-bit message type and one reserved byte. V2 senders SHALL set the magic to `0x44434632` (`DCF2`), the payload version to `13` and the reserved byte to 0. Packets SHALL be sent as the raw memory image of the shared structs as laid out by the ESP32 toolchain; there is no portable serialisation.

#### Scenario: V2 sender builds a header
- **WHEN** the Mainframe builds any protocol packet
- **THEN** the header contains magic `0x44434632`, version `13`, the message type of the packet and a zero reserved byte

#### Scenario: Header size is fixed
- **WHEN** the shared protocol header is compiled for host tests
- **THEN** it is exactly 8 bytes long

### Requirement: Message types and sizes

The protocol SHALL define exactly these message types, each with a fixed packet size including the header: `SceneSelect` = 1 (12 bytes), `ClockSync` = 2 (16 bytes), `MainframeHello` = 3 (8 bytes), `NodeText` = 4 (60 bytes) and `NodeVisualState` = 5 (20 bytes). All five SHALL travel from the Mainframe to all Nodes by broadcast.

#### Scenario: Layouts stay stable
- **WHEN** the host tests compile the shared packet structs
- **THEN** their sizes are 12, 16, 8, 60 and 20 bytes for SceneSelect, ClockSync, MainframeHello, NodeText and NodeVisualState

#### Scenario: Unknown message type
- **WHEN** a Node receives a packet whose message type is none of the five defined types
- **THEN** it ignores the packet without any side effect

### Requirement: Receive validation before side effects

A Node SHALL accept a packet only if its length is exactly the size defined for its message type, its magic is `0x44434632`, its payload version is `13` and its message type matches the expected type. The Node SHALL drop any packet that fails one of these checks before staging it or changing any state.

#### Scenario: Packet shorter than a header
- **WHEN** a Node receives a packet shorter than 8 bytes
- **THEN** it ignores the packet

#### Scenario: Wrong length for the type
- **WHEN** a Node receives a packet with type `ClockSync` that is not exactly 16 bytes long
- **THEN** it ignores the packet

#### Scenario: Wrong payload version
- **WHEN** a Node receives a packet with magic `0x44434632` and a payload version other than `13`
- **THEN** it ignores the packet

#### Scenario: Wrong magic
- **WHEN** a Node receives a correctly sized packet whose magic is not `0x44434632`
- **THEN** it ignores the packet

### Requirement: Mutual isolation of V1 and V2

V2 receivers SHALL reject every packet carrying the V1 magic `0x4443464C` (`DCFL`), for every message type. V2 senders SHALL never send the V1 magic, so that V1 receivers, which require `0x4443464C`, reject every V2 packet. The installation identity SHALL NOT be treated as authentication.

#### Scenario: V1 packet reaches a V2 Node
- **WHEN** a V2 Node receives a correctly sized packet with magic `0x4443464C` and version `13` for any message type
- **THEN** it ignores the packet

#### Scenario: V2 packet reaches a V1 receiver
- **WHEN** a V1 receiver checks the header of any V2 packet against its frozen contract (magic `0x4443464C`, version `13`, matching type)
- **THEN** the header check fails

#### Scenario: Message type unknown to V1
- **WHEN** a `NodeVisualState` packet is relabelled with the V1 magic
- **THEN** both V2 and the frozen V1 contract reject it, because V2 requires `DCF2` and V1 does not know message type 5

### Requirement: One-way protocol without delivery guarantees

Nodes SHALL NOT transmit any protocol packet: no acknowledgements, status, heartbeat or clock replies. The protocol SHALL have no sessions, no pairing, no encryption, no sender binding and no application-level retransmission other than the periodic repeats defined in this spec. A successful send call on the Mainframe SHALL NOT be taken as proof that any Node received the packet.

#### Scenario: Mainframe does not know which Nodes are present
- **WHEN** the Mainframe sends a scene selection
- **THEN** it receives no reply from any Node and does not track which Nodes applied the scene

#### Scenario: Packet from an unknown sender
- **WHEN** a Node receives a valid V2 packet from any sender MAC address
- **THEN** it processes the packet; the sender address is not checked

#### Scenario: Mainframe radio not ready
- **WHEN** the Mainframe tries to send while its radio is not ready
- **THEN** the packet is not sent and no retry is queued for it

### Requirement: Node receive staging

The ESP-NOW receive handler on a Node SHALL only validate and stage packets; the main loop SHALL process them. The Node SHALL keep one staging slot per message type, so a newer valid packet of a type replaces an unprocessed older one of the same type. In one pass the main loop SHALL process staged packets in the order MainframeHello, SceneSelect, ClockSync, NodeText, NodeVisualState.

#### Scenario: Two clock packets before the loop runs
- **WHEN** two valid ClockSync packets arrive before the Node main loop processes radio input
- **THEN** only the newer ClockSync is applied

#### Scenario: Hello and clock in the same pass
- **WHEN** a MainframeHello and a ClockSync are both staged when the main loop processes radio input
- **THEN** the Hello is processed first and the ClockSync afterwards, so the Node ends in `MainframeRunning`

### Requirement: Mainframe hello

The Mainframe SHALL send `MainframeHello` exactly once, when its radio becomes ready during boot. The hello SHALL carry no payload, session or revision. It SHALL NOT be repeated and SHALL NOT be required before a Node accepts scenes or clock.

#### Scenario: Boot with working radio
- **WHEN** the Mainframe boots and its radio and broadcast peer are ready
- **THEN** it sends one MainframeHello before the show is started

#### Scenario: No repeated hello
- **WHEN** the show is started or scenes change later
- **THEN** the Mainframe sends no further MainframeHello until it reboots

### Requirement: Node reaction to hello

On a valid `MainframeHello` a Node SHALL clear any text overlay, run a visible connect confirmation sequence, switch its output to the idle command for its role and enter `MainframeWaiting`, from any run mode. Entering `MainframeWaiting` SHALL clear the record of the last rendered beat.

#### Scenario: Hello reaches a Node in demo
- **WHEN** a Node in `Demo` processes a MainframeHello
- **THEN** it shows the connect confirmation, switches to idle output and enters `MainframeWaiting`

#### Scenario: Hello reaches a running Node
- **WHEN** a Node in `MainframeRunning` processes a MainframeHello, for example after the Mainframe rebooted
- **THEN** it enters `MainframeWaiting` and stops rendering beats until the next ClockSync

### Requirement: Scene selection layout

A `SceneSelect` packet SHALL contain the header, a `uint8` scene index and three reserved bytes set to 0. The scene index SHALL refer to the scene table compiled into both Mainframe and Nodes; the Mainframe SHALL NOT send role commands, pixel data or scene contents.

#### Scenario: Scene packet content
- **WHEN** the Mainframe selects scene index 1
- **THEN** it broadcasts a 12-byte SceneSelect whose scene index is 1 and whose reserved bytes are 0

### Requirement: Mainframe scene sending

The Mainframe SHALL send `SceneSelect` with the current scene index when the show starts, on every scene change, and again periodically while the show runs. Every SceneSelect send SHALL restart the repeat period. Before the show starts the Mainframe SHALL NOT send SceneSelect. The repeat period is a timing value, documented under Current values.

#### Scenario: Show start
- **WHEN** the show is not running and the user short-presses the Mainframe button
- **THEN** the Mainframe starts the show and sends SceneSelect for the current scene, followed by a ClockSync

#### Scenario: Scene change
- **WHEN** the show is running and the user short-presses the Mainframe button
- **THEN** the Mainframe advances to the next scene index, wrapping to 0 after the last scene, sends SceneSelect once and restarts the repeat period

#### Scenario: Periodic repeat
- **WHEN** the show is running and the repeat period has passed since the last SceneSelect
- **THEN** the Mainframe sends the same SceneSelect again

### Requirement: Node scene handling

A Node SHALL ignore a `SceneSelect` whose index is not below its compiled scene count. Otherwise it SHALL make that scene current and derive its own role-specific command from the scene table. Applying a scene whose derived command equals the active command SHALL NOT restart the output. Scene handling SHALL NOT change the Node run mode.

#### Scenario: Repeated scene
- **WHEN** a Node receives a SceneSelect for the scene that is already active
- **THEN** its output continues without a restart

#### Scenario: Scene index out of range
- **WHEN** a Node receives a SceneSelect whose index is equal to or greater than its scene count
- **THEN** it ignores the packet

#### Scenario: Scene while in demo
- **WHEN** a Node in `Demo` receives a valid SceneSelect
- **THEN** it applies the scene and stays in `Demo`

### Requirement: Clock sync layout

A `ClockSync` packet SHALL contain the header, a `uint16` BPM, a `uint8` beats per bar, a `uint8` beat in bar counted from 1 and a `uint32` bar number counted from 1. It SHALL carry no sender timestamp.

#### Scenario: Clock packet content
- **WHEN** the Mainframe sends a clock sync at 120 BPM on beat 1 of bar 3 with four beats per bar
- **THEN** the 16-byte packet carries BPM 120, beats per bar 4, beat in bar 1 and bar 3

### Requirement: Mainframe clock sending

While the show runs, the Mainframe SHALL send `ClockSync` with the current show tempo at show start, on beat 1 of every bar, and immediately on every tempo adoption. The show starts at bar 1, beat 1 with four beats per bar. A tempo adoption SHALL move the show clock to beat 1 of the next bar before the ClockSync is sent. Before the show starts the Mainframe SHALL NOT send ClockSync. When several missed beats are counted at once, ClockSync SHALL be sent only if the last one is beat 1.

#### Scenario: Beat 1 of a bar
- **WHEN** the show clock reaches beat 1 of a bar
- **THEN** the Mainframe sends ClockSync with beat in bar 1 and the current bar number

#### Scenario: Other beats
- **WHEN** the show clock reaches beat 2, 3 or 4 of a bar
- **THEN** the Mainframe sends no ClockSync for that beat

#### Scenario: Tempo adoption
- **WHEN** the show is running and the tempo adoption accepts a new BPM
- **THEN** the Mainframe sets the show tempo, advances to beat 1 of the next bar and sends ClockSync immediately

#### Scenario: Bar start among missed beats
- **WHEN** the show clock counts several missed beats at once, passes beat 1 of a bar and ends on beat 2
- **THEN** the Mainframe sends no ClockSync for that bar

#### Scenario: Missed beats ending on a bar start
- **WHEN** the show clock counts several missed beats at once and the last of them is beat 1 of a bar
- **THEN** the Mainframe sends one ClockSync with beat in bar 1 and the current bar number

### Requirement: Node clock adoption

A Node SHALL ignore a `ClockSync` whose BPM is 0. Otherwise it SHALL adopt the BPM and beats per bar, using four beats per bar when the packet carries 0, and enter `MainframeRunning` from any run mode, with or without a previous hello.

#### Scenario: Clock without hello
- **WHEN** a Node in `Demo` receives a valid ClockSync with non-zero BPM
- **THEN** it adopts the tempo, renders the received beat and enters `MainframeRunning`

#### Scenario: Zero BPM
- **WHEN** a Node receives a ClockSync with BPM 0
- **THEN** it ignores the packet and keeps its run mode and clock

### Requirement: Node beat alignment on clock sync

On an adopted `ClockSync`, if the same beat and bar was rendered within a short duplicate window, the Node SHALL only reschedule its next beat. Otherwise it SHALL adopt beat in bar and bar number, using 1 for a 0 value, and render that beat at once. In both cases the next beat SHALL follow one beat interval after the packet was processed.

#### Scenario: Duplicate beat
- **WHEN** a Node receives a ClockSync for the beat and bar it rendered moments ago, within the duplicate window
- **THEN** it does not render that beat again and schedules its next beat one interval after processing

#### Scenario: Zero fields
- **WHEN** a Node receives a ClockSync with non-zero BPM and beats per bar, beat in bar or bar number set to 0
- **THEN** it uses four beats per bar, beat 1 or bar 1 respectively

### Requirement: Node run modes

A Node SHALL have three run modes. `Demo` SHALL be the mode after boot: the Node plays its local scene with its own clock, and a short button press selects the next local scene only in this mode. `MainframeWaiting` SHALL be entered only through a hello; in it the Node SHALL render no beats until a ClockSync arrives. `MainframeRunning` SHALL be entered only through a valid ClockSync; between syncs the local clock SHALL keep running.

#### Scenario: Boot
- **WHEN** a Node boots
- **THEN** it is in `Demo`, plays scene 0 and runs its own default clock

#### Scenario: Waiting Node without clock
- **WHEN** a Node is in `MainframeWaiting` and no ClockSync arrives
- **THEN** it renders no beats and stays in `MainframeWaiting`

#### Scenario: Button press outside demo
- **WHEN** a Node in `MainframeWaiting` or `MainframeRunning` gets a short button press
- **THEN** it does not change the scene

### Requirement: No automatic return to demo

A Node that has left `Demo` SHALL stay under Mainframe control until it reboots. Radio silence shorter than the reboot period SHALL NOT change its run mode, and the Node SHALL have no holdover state. The only automatic way back to `Demo` SHALL be the reboot after long silence defined in this spec.

#### Scenario: Radio loss while running
- **WHEN** a Node in `MainframeRunning` receives no packets for less than the reboot period
- **THEN** it keeps running with the last tempo and scene and does not fall back to `Demo`

#### Scenario: Radio loss while waiting
- **WHEN** a Node in `MainframeWaiting` receives no packets for less than the reboot period
- **THEN** it stays in `MainframeWaiting` without rendering beats

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

### Requirement: Node text layout and handling

A `NodeText` packet SHALL contain the header, a target Node kind, a flags byte whose bit 0 means cancel, two reserved bytes and a 48-byte text field holding at most 47 characters followed by zero bytes. A Node SHALL ignore NodeText addressed to another Node kind. A cancel SHALL clear the text overlay in any run mode; other text SHALL be queued for display only in `MainframeRunning` and ignored otherwise. The V2 Mainframe does not send NodeText yet.

#### Scenario: Text for another Node kind
- **WHEN** a flashlight Node receives a NodeText whose target kind is RGB strip
- **THEN** it ignores the packet

#### Scenario: Cancel
- **WHEN** a Node receives a NodeText for its kind with the cancel flag set
- **THEN** it clears its text overlay, whatever its run mode

#### Scenario: Text before clock
- **WHEN** a Node in `Demo` or `MainframeWaiting` receives a NodeText for its kind without the cancel flag
- **THEN** it does not show the text

#### Scenario: Long text is truncated
- **WHEN** a NodeText packet is built from a text longer than 47 characters
- **THEN** the text field holds the first 47 characters followed by zero bytes

### Requirement: Node visual state layout

A `NodeVisualState` packet SHALL contain the header and a 12-byte neutral render directive: RGB mode (0 = scene, 1 = solid), brightness in percent, a solid colour as red, green and blue, an overlay colour as red, green and blue, overlay opacity in percent, a flash override (0 = scene, 1 = off, 2 = full) and one reserved byte. It SHALL carry output instructions only, never mood values.

#### Scenario: Visual state content
- **WHEN** the Mainframe requests solid red with full flash
- **THEN** the 20-byte packet carries RGB mode 1, colour 255/0/0 and flash override 2

### Requirement: Mainframe visual state sending

Once its radio is ready, the Mainframe SHALL send `NodeVisualState` whenever the directive derived from its mood changes, and again periodically while it stays the same, whether or not the show runs. A directive SHALL count as sent only when the send call succeeds; otherwise the Mainframe SHALL try again on a later loop pass. The refresh period is a timing value, documented under Current values.

#### Scenario: Directive changes
- **WHEN** the directive derived from the mood differs from the last one sent successfully
- **THEN** the Mainframe sends the new directive at once

#### Scenario: Unchanged directive
- **WHEN** the directive stays the same and the refresh period has passed since the last successful send
- **THEN** the Mainframe sends the same directive again

#### Scenario: Before the show starts
- **WHEN** the Mainframe radio is ready and the show has not started
- **THEN** the Mainframe still sends NodeVisualState

### Requirement: Node visual state handling

A Node SHALL drop a `NodeVisualState` whose RGB mode or flash override is not a defined value, or whose brightness or overlay opacity is above 100. A valid directive SHALL be applied in any run mode and SHALL reset any running flash burst.

#### Scenario: Invalid directive
- **WHEN** a Node receives a NodeVisualState with brightness 101
- **THEN** it ignores the packet

#### Scenario: Valid directive in demo
- **WHEN** a Node in `Demo` receives a valid NodeVisualState
- **THEN** it applies the directive to its output

## Current values

These are the values in the code on 2026-11-14. They are calibration and may change without a spec change; the protocol constants above (magic, version, types, sizes) are contract and may not.

- Scene repeat period: 30 s (`kSceneRefreshMs`, Mainframe).
- Visual state refresh period: 3 s (`kLightingRefreshMs`, Mainframe).
- Show start tempo: 120 BPM (`kDefaultBpm`, Mainframe); Node demo clock: 120 BPM (`BPM`, Node).
- Node duplicate-beat window: 250 ms (`DUPLICATE_BEAT_GUARD_MS`).
- Node connect confirmation: three flashes of 260 ms, 1 s apart.
- Node radio silence period: 30 s (`RADIO_SILENCE_PERIOD_MS`); Node reboot period: 5 minutes of silence (`RADIO_REBOOT_PERIOD_MS`). Added 2026-11-17, reasons in the archived change `fix-node-radio-recovery`, design.md.
