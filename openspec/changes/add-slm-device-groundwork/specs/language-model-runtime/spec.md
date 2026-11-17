# Spec Delta

## Purpose

The on-device language model runtime of the Mainframe: where the model image is stored, how it is loaded, where it computes and how it is kept apart from audio, show clock, radio and display. What the model generates and how texts reach the message panel is not part of this spec yet.

## ADDED Requirements

### Requirement: Runtime only in the measurement build

The language model runtime SHALL exist only in a dedicated measurement build of the Mainframe. The normal Mainframe build SHALL contain no model runtime, no model partition and no model task.

#### Scenario: Normal build
- **WHEN** the normal Mainframe firmware runs
- **THEN** no model image is loaded and no model task exists

### Requirement: Model image in its own flash partition

The measurement build SHALL read the model from one dedicated flash partition, mapped in place. It MAY unpack weights from the image into PSRAM at start; it SHALL NOT copy model data of that size into internal RAM. The firmware SHALL fit in a single app slot beside it.

#### Scenario: Model present
- **WHEN** the measurement build boots with a valid model image in the model partition
- **THEN** the runtime reports the image as loaded, PSRAM use grows by less than the size of the image, and internal RAM use stays far below it

### Requirement: Missing or invalid model

If the model partition is missing, empty or holds an image the runtime does not recognise or cannot fit into its working memory, the runtime SHALL stay off and report the reason on the serial line. The show, the eye, the panel and the radio SHALL run as in the normal build.

#### Scenario: Empty partition
- **WHEN** the measurement build boots with an erased model partition
- **THEN** it reports that no model was found, starts no generation and runs the show normally

#### Scenario: Working memory unavailable
- **WHEN** the working memory for the model cannot be allocated
- **THEN** the runtime stays off, reports it, and the show runs normally

### Requirement: Generation in the background

The runtime SHALL generate in a task of its own, below the priority of the radio stack and of the main loop. The task SHALL give up the processor at regular short intervals, so that no watchdog fires and higher-priority work is never held up by it. The main loop SHALL never wait for the model.

#### Scenario: Continuous generation
- **WHEN** the model generates without pause for several minutes while the show runs with music
- **THEN** no watchdog reset occurs and the main loop keeps its pass rate within the limits recorded in design.md

#### Scenario: Radio work while generating
- **WHEN** the radio stack has work while the model computes on the same core
- **THEN** the radio work runs first

### Requirement: Isolation from the show

Model output SHALL NOT reach the message panel, the Nodes, the personality, the show clock or the tempo path. The runtime SHALL only report measurements (time per output piece, memory use, state) on the serial line.

#### Scenario: Generated text
- **WHEN** the model has generated a text
- **THEN** the panel shows only texts from its fixed lists and nothing is sent to Nodes because of it

### Requirement: Offline operation

The runtime SHALL work without access point, internet or any external service. It SHALL NOT start Wi-Fi association or change the radio channel.

#### Scenario: No network
- **WHEN** the measurement build runs with no access point in range
- **THEN** the model loads and generates as with one, and ESP-NOW stays on its channel
