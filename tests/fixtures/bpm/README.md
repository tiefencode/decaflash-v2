# Atom BPM reference traces

These CSV files are 12-second middle sections of the 2026-10-01 Atom feature
capture. Each row contains the timestamp in milliseconds, the absolute block
level, and the percussive level supplied to the BPM tracker at a 16 ms cadence.

They intentionally retain the device's measured feature values rather than
synthetic pulses. `tests/run_bpm_trace_report.sh` replays them on the host and
reports an estimator's correct, half-tempo, other, and unresolved evaluations.

| file | hand-tapped reference BPM | source sequence window |
| --- | ---: | ---: |
| `atom-98.csv` | 98 | 1500–2249 |
| `atom-100.csv` | 100 | 28000–28749 |
| `atom-120.csv` | 120 | 21000–21749 |
| `atom-160.csv` | 160 | 14500–15249 |
| `atom-180.csv` | 180 | 8500–9249 |
