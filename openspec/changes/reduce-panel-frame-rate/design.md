# Design

Every number below is marked: **[measured]** on the device on 2026-11-17 (env `mainframe-loop-load` or `mainframe-panel-rate-sweep`) or **[computed]**.

## Context

- Everything of ours runs in `loop()` on core 1. Core 1 busy share: 55 % idle, 59 % show with music and no text, 75 % while a text is visible throughout **[measured]**.
- Eye frame: 11.8–12.0 ms without text, 17.6 ms with text, peaks up to 61 ms, about 35 frames/s; eye share of core 1: 41 % / 43 % / 62 % **[measured]**. The frame interval is 25 ms (`kFrameIntervalMs`).
- Largest gap between audio blocks: 54 ms without text, 94 ms with text; between IMU samples: 32 ms idle, 37 ms show, 71 ms with text. No audio drops, backlog at most 6 of 16 blocks **[measured]**.

## Goals / Non-Goals

**Goals:** lower the load while a text is visible, with a frame rate the user accepts on the device.

**Non-Goals:** no other tuning of the eye renderer, no change to `MessagePanel`, the audio path or the radio.

## Decisions

### 1. Where the reduced rate applies (user decision, 2026-11-17)

Only in frames where the panel is actually drawn, not during sleep or debug screens, and in the normal `mainframe` build. The renderer already knows per frame whether it draws the panel, so the interval is picked there. The condition and the selection are in `frame_interval.h` (`drawsMessagePanel`, `selectFrameIntervalMs`), shared by the renderer and the loop-load report and covered by a host test.

### 2. Value: 67 ms under the panel (user decision, 2026-11-17)

The interval under the panel is 67 ms, about 15 frames/s (`kPanelFrameIntervalMs` in `apps/mainframe/src/frame_interval.h`). The normal interval stays 25 ms.

How it was chosen:

- Look: a demo build ran the eye without overlay at 25, 40, 50, 67 and 100 ms. The user judged 67 ms "still super fine, especially behind the overlay"; 100 ms was not accepted.
- Numbers: one sweep capture (build `mainframe-panel-rate-sweep`, same music, show running, windows with a text visible throughout) **[measured]**:

| Interval under the panel | Windows | Busy share core 1 | Eye frames/s | Audio gap max / p95 / median | IMU gap max / p95 / median |
|---|---|---|---|---|---|
| 25 ms (before) | 69 | 73.7 % | 34.4 | 92 / 60 / 54 ms | 65 / 36 / 34 ms |
| 40 ms | 55 | 57.2 % | 23.3 | 49 / 49 / 46 ms | 38 / 35 / 33 ms |
| 50 ms | 45 | 50.9 % | 18.9 | 58 / 56 / 52 ms | 42 / 38 / 33 ms |
| 67 ms | 46 | 45.1 % | 14.6 | 72 / 57 / 52 ms | 65 / 39 / 33 ms |

- Reasoning: 40 ms already removes the large audio and IMU gaps with a text; longer intervals do not shrink the gaps further (the medians follow the audio block cadence), but they free more of core 1. 67 ms gives the most headroom at a look the user accepts. The single maxima at 67 ms fall in windows where a new text with an emoji appears; such peaks of about 60 ms also occur at 25 ms.
- The scroll step (0.8 s) and the more-text blink (450 ms) are slower than 67 ms, so the panel itself is unaffected **[computed]**.

For a later decision (not part of this change), the same capture has the show without text at 40 ms: busy share 47.1 % against 58.8 % at 25 ms, 23.9 against 35.8 frames/s **[measured]**.

### 3. Measurement method

The candidates were compared in one capture with `mainframe-panel-rate-sweep` (`mainframe-loop-load` plus a sweep starting 80 s after the show: a fixed demo text at 25, 40, 50 and 67 ms under the panel, then no text at 25 and 40 ms, 15 s each with 3 s at the normal intervals in between). This replaces separate runs per candidate: same session, same music, and the 25 ms rows are the baseline before the step. `LOOP_A` reports `frame_ms` and `panel_ms`, so every one-second window is assigned to its step; `docs/panel-rate-captures/summarize.py` does the assignment.

The baseline after the step is taken with the normal `mainframe-loop-load` build in the three states of the 2026-11-17 run (about 50 s idle, 80 s show with music, then a text phase with texts triggered by knocking), plus a debug screen while a text is active. Maxima are noisy; medians and p95 are given as well. Opening the serial port did not restart the Mainframe in these captures.

## Risks / Trade-offs

- [Lower frame rate looks wrong] The eye under the panel may look jerky. → The user judged the look on the device before choosing.
- [Gain smaller than computed] The earlier estimates were computed. → Measured: 45.1 % at 67 ms against the computed 45–50 % for 50 ms.
- [Longer wait for the first panel frame] A new text appears with the next frame; at 67 ms that is at most one interval later than before. Not measured separately.

## Migration Plan

Only the frame interval changes; rollback is reverting that commit.
