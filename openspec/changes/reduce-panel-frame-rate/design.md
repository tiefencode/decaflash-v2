# Design

Every number below is marked: **[measured]** on the device on 2026-11-17 (branch `claude/loop-load-measurement-f3ac43`, env `mainframe-loop-load`) or **[computed]**.

## Context

- Everything of ours runs in `loop()` on core 1. Core 1 busy share: 55 % idle, 59 % show with music and no text, 75 % while a text is visible throughout **[measured]**.
- Eye frame: 11.8–12.0 ms without text, 17.6 ms with text, peaks up to 61 ms, about 35 frames/s; eye share of core 1: 41 % / 43 % / 62 % **[measured]**. The frame interval is 25 ms (`kFrameIntervalMs`).
- Largest gap between audio blocks: 54 ms without text, 94 ms with text; between IMU samples: 32 ms idle, 37 ms show, 71 ms with text. No audio drops, backlog at most 6 of 16 blocks **[measured]**.

## Goals / Non-Goals

**Goals:** lower the load while a text is visible, with a frame rate the user accepts on the device.

**Non-Goals:** no other tuning of the eye renderer, no change to `MessagePanel`, the audio path or the radio.

## Decisions

### 1. Where the reduced rate applies (user decision, 2026-11-17)

Only in frames where the panel is actually drawn, not during sleep or debug screens, and in the normal `mainframe` build. The renderer already knows per frame whether it draws the panel, so the interval is picked there.

### 2. The value is measured first, then chosen by the user

Candidates to measure (calibration, not requirement text):

| Interval under the panel | Frames/s | Busy share of core 1 with text |
|---|---|---|
| 25 ms (today) | about 35 **[measured]** | 75 % **[measured]** |
| 40 ms | about 25 **[computed]** | about 65 % **[computed]** |
| 50 ms | about 18–20 **[computed]** | about 45–50 % **[computed]** |

The user picks the value after seeing the numbers and the look on the device; value and reasoning are then recorded here. The scroll step (0.8 s) and the more-text blink (450 ms) are far slower than any candidate, so the panel itself stays readable **[computed]**; whether the eye under the shade looks acceptable can only be judged on the device.

### 3. Measurement method

The loop-load report (`-DDECAFLASH_LOOP_LOAD=1`) in three states: 50 s idle, 80 s show with music, 95 s text phase, same music for every run. Maxima are noisy, so candidates are compared with a baseline taken in the same session. Reading the serial port restarts the Mainframe and is asked for before each capture.

## Risks / Trade-offs

- [Lower frame rate looks wrong] The eye under the panel may look jerky. → The user judges on the device and picks the value, or drops the change.
- [Gain smaller than computed] The busy shares of the candidates are computed, not measured. → Task 2.2 measures them before anything is decided.

## Migration Plan

Only the frame interval changes; rollback is reverting that commit.
