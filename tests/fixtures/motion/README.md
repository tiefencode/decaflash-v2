# Motion captures

Serial output of the `mainframe-motion-capture` build, `MOTION_*` lines only.
Line formats are defined in `apps/mainframe/src/main.cpp` and
`apps/mainframe/src/motion_trace.h`. `mg` is acceleration above the slow
gravity estimate, the value the classifier calls `dynamic`.

## 2026-11-16-wood-sphere.log

AtomS3R mounted in a wooden sphere of about 3 kg, audio analysis running.
Gestures were performed one block at a time; `t` is device uptime in ms.

| t (s) | Gesture | Windows | Peak | Time at or above 450 mg | Rotation |
| --- | --- | --- | --- | --- | --- |
| 131–157 | soft knock | 4–19 | 80–340 mg | 0 | 5–40 °/s |
| 195–226 | normal knock | 21–37 | 230–600 mg | 0–25 ms | 30–75 °/s |
| 256–289 | hit | 41–60 | 0.7–2.6 g | 70–265 ms | 80–360 °/s |
| 337–354 | shake | 65–83 | 1.5–2.2 g | 450–700 ms over several windows | 325–570 °/s |
| 362–386 | roll | 87–113 | 0.4–1.1 g | 0–700 ms over several windows | 80–180 °/s |
| 387–530 | rest | idle lines | 2–29 mg | 0 | 0–1 °/s |

Small windows right after a gesture (peak below about 60 mg) are its decay.

Rest was measured without music up to about t = 475 s and with music at
living-room volume afterwards; both show the same floor. Show volume
(about 90 dB) has not been measured.

The classifier of that date (`MOTION_CLS`) reported none of the soft knocks,
2 of 15 normal knocks as tap and 2 of 14 hits as impact.
