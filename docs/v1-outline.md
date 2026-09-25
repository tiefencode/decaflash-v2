# Decaflash V1 – historischer Entwurf

Dieser Entwurf beschreibt einen frühen V1-Stand und ist keine aktuelle Bestandsaufnahme oder Aufgabenliste. Einstieg: [README](../README.md); aktuelle Entscheidungen und Analyse: [Mainframe V2](MAINFRAME_V2.md).

## Scope

V1 focuses on the flashlight node as a standalone device.

- no radio yet
- no microphone yet
- no beat detection from audio yet
- no RGB node yet

## Device split

- `mainframe`: placeholder firmware for the future controller cube
- `node`: standalone flashlight firmware for the Atom Lite + Flashlight Unit

## Node V1 goals

- boot reliably
- run three musically useful flashlight programs locally
- allow local mode switching with the ATOM button
- keep shared node logic separate from hardware-specific rendering
- prepare shared types for later radio commands

## Next steps

1. add local default preset selection
2. save the selected default preset to persistent storage
3. add ESP-NOW transport between mainframe and node
4. add microphone input on the mainframe
