# CLAUDE.md

All project rules are in AGENTS.md and apply to every coding agent. Add rules there, not here.

@AGENTS.md

## Claude Code specifics

- `.claude/settings.json` enforces part of those rules: builds, host tests and read-only Git commands run without a prompt; `pio run` with a target (upload) and `pio device monitor` ask first; the local secret files cannot be read or edited. The rules in AGENTS.md also apply where the settings do not enforce them. Do not route around a prompt or a denial with a different command.
