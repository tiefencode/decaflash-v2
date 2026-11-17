# Proposal

## Why

While a text is visible on the message panel, core 1 is 75 % busy and the largest gap between audio blocks grows from 54 ms to 94 ms (measured on the device, 2026-11-17). The eye is redrawn at the full rate underneath the panel. Drawing fewer frames in that state lowers the load for today's firmware and leaves room for later features.

## What Changes

- The eye draws at a lower frame rate in frames where the message panel is drawn over it. This is product behaviour in the normal `mainframe` build.
- The rate is chosen by the user after a device measurement of several candidates and a look at the device. The user chose 67 ms (about 15 frames/s) on 2026-11-17.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `eye-iris`: adds a reduced frame rate for frames in which the message panel is drawn.

## Impact

- `apps/mainframe/src/eye_renderer.{h,cpp}` and the new `frame_interval.h`: frame interval depends on whether the panel is drawn; host test `tests/frame_interval.cpp`.
- Uses the loop-load instrumentation (env `mainframe-loop-load`, already on `main`); its report gains `frame_ms` and `panel_ms`. New measurement-only env `mainframe-panel-rate-sweep`.
- `shared/include`, the radio protocol and the Node firmware are not changed. No dependency pin changes.
- Docs: `docs/project-context.md` (measurement results). `MessagePanel` and its spec are unchanged.
- The language model prototype (`docs/slm-prototype.md`) uses the baseline taken at the end of this change; this change does not depend on the prototype.
