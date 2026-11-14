# CLAUDE.md

The shared project rules for all coding agents are in AGENTS.md. Keep them there instead of duplicating them here.

@AGENTS.md

## Claude Code specifics

- Run `sh tests/run_all.sh` for all host tests. After firmware changes, also build each affected environment with `pio run -e <env>`.
- Do not read `apps/mainframe/src/panel_text_sprites.h`. It is a generated sprite atlas of about 7,000 lines and costs a lot of context.
- Flashing (`pio run -t upload`) and serial monitoring need explicit approval from the user every time. A successful build is not a device test.
- `docs/MAINFRAME_V2.md` is large. Read only the section the task needs.
