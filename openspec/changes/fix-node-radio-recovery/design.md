# Design

## Context

See proposal.md for the motivation. Facts from the code that shape the approach:

- The Node calls `initEspNow()` and `ensureBroadcastPeer()` once in `setup()` and registers `onEspNowReceive()`. Nothing touches the radio afterwards.
- `recoverEspNow()` in `shared/include/espnow_transport.h` exists and is called nowhere. It sets station mode and channel 1 again, runs `esp_now_deinit()` / `esp_now_init()` and adds the broadcast peer. It does not register a receive handler; `esp_now_deinit()` drops the registered one.
- While it runs, the Mainframe sends `NodeVisualState` at least every 3 s (`kLightingRefreshMs`), also before the show starts. A Node in range therefore sees a valid packet at least every 3 s.
- `onEspNowReceive()` runs in the Wi-Fi task; the main loop reads staged data under `radioMux`.
- The cause of BUG-014 is not measured. Known only: the Node's main loop kept running, packets had no effect, a Node reset helped. Which layer was stuck (ESP-NOW, Wi-Fi driver, channel) is unknown.

## Goals / Non-Goals

**Goals:**

- A stuck receive path repairs itself without a visible break in the output.
- The serial log of a repair tells which layer was stuck: the channel before the repair, and which stage brought packets back.
- The decision logic is testable on the host.

**Non-Goals:**

- Finding the cause of BUG-014. The log only narrows it.
- Any Mainframe change, any Node transmission (see proposal.md).
- A watchdog for a hung main loop. If the loop hangs (for example the 0 ms beat interval of BUG-001), this recovery does not run either.

## Decisions

### Silence watchdog as a hardware-independent class

`apps/node/src/radio_watchdog.h`, header only, no Arduino or ESP-IDF includes. It takes `begin(now)`, `onPacket(now)` and `poll(now, underMainframeControl)` and returns `None`, `ReinitEspNow`, `RestartWifi` or `Reboot`. It is armed from `begin()`; the silence counts from boot until the first packet. After a silence period without `onPacket()` it returns `ReinitEspNow` once, then `RestartWifi` once per further period. Once the silence reaches the reboot period it returns `Reboot` if the caller reports Mainframe control, and keeps returning `RestartWifi` per period otherwise. `onPacket()` resets the sequence and reports, on the next `poll()`, that packets returned and after how many attempts. All time arithmetic is unsigned subtraction, so a `millis()` overflow is harmless.

Alternative considered: put the timing into `main.cpp`. Rejected: `main.cpp` is already too large (ideas.md) and the logic would not be host-testable.

### Values

- Armed from boot (user decision, 2026-11-17): a Node whose receive path dies before it ever heard the Mainframe must recover too.
- Reboot period: 5 minutes of continuous silence, only in `MainframeWaiting` or `MainframeRunning` (user decision, 2026-11-17). By then about nine repairs have run. Five minutes is short against a show and long against any blocking output or a Mainframe restart.
- Silence period: 30 s (user decision, 2026-11-17). Ten missed 3-s refreshes in a row; the user preferred fewer needless repairs under bad reception over a faster repair.
- Repeat: every 30 s, without limit and without growing gaps (user decision, 2026-11-17).
- Stages: attempt 1 ESP-NOW re-initialisation, attempt 2 and later Wi-Fi restart (user decision, 2026-11-17).

### What counts as a packet

Any packet that passes `isValidHeader()` for its type, counted in `onEspNowReceive()` before the Node-kind filter for `NodeText`. A text for the other Node kind still proves the receive path works. The handler only stores `millis()` and a flag under `radioMux`; the main loop hands them to the watchdog.

### Stage 1: `recoverEspNow()` plus handler registration

Reuse the existing shared function unchanged and call `esp_now_register_recv_cb()` afterwards. The shared header stays untouched, so the Mainframe binary does not change and needs no flashing.

### Stage 2: full Wi-Fi restart in the Node

New `apps/node/src/radio_recovery.{h,cpp}`: `esp_now_deinit()`, `esp_wifi_stop()`, `esp_wifi_deinit()`, then `initEspNow()`, `ensureBroadcastPeer()` and the handler registration, the same calls as at boot. This is the closest the Node gets to a reset without rebooting.

Alternative considered: `esp_wifi_stop()` / `esp_wifi_start()` only. Rejected because the stuck layer is unknown and a reset is the only repair known to work; the full driver re-initialisation is nearer to it. If the device test shows a visible output glitch, fall back to stop/start and record the measurement here.

Both stages run in the main loop, not in the receive handler.

### Reboot safeguards

Agreed with the user on 2026-11-17 as the meaning of "fail-safe":

- Only after the reboot period of continuous silence; one valid packet starts it again.
- Only under Mainframe control. After the reboot the Node is in `Demo`; it leaves `Demo` only through a received hello or clock, so a second reboot needs a working radio first. A loop is impossible by construction, without a counter in NVS.
- Directly before `ESP.restart()` the main loop reads the staged packet timestamp under `radioMux` once more and cancels if a packet arrived.
- `renderer.allOff()` before the restart, so no flash or strip stays lit across it.
- Node kind and role are read from NVS at boot as today; boot does not depend on the radio. If the radio start-up fails, the Node plays `Demo` and the watchdog repairs after 30 s.
- The decision is part of the host-tested watchdog, including `millis()` overflow.

### Serial output

- Per repair: `RADIO: silent=<s>s attempt=<n> channel=<c> action=espnow_reinit|wifi_restart result=ok|failed took=<ms>ms heap=<bytes>` followed, on failure, by a `RADIO: codes ...` line with the error codes of the steps. `took` and `heap` (free heap after the repair) are there for the device test.
- On return: `RADIO: packets_back silent=<s>s attempts=<n>`.
- Before a reboot: `RADIO: silent=<s>s action=reboot`. At boot: `BOOT: reset_reason=<n>` from `esp_reset_reason()`.
- `status`: `RADIO: ready=<0|1> channel=<c> last_packet_age=<s>s|never repairs=<n>`.

The channel comes from `esp_wifi_get_channel()` and is read before the repair, so a drifted channel is visible.

## Risks / Trade-offs

- [The fault of BUG-014 sits where neither repair stage reaches] → The reboot after 5 minutes covers it for a Node under Mainframe control. The log then shows repairs without `packets_back`, the reboot line and packets after the boot.
- [The receive path dies while the Node is in `Demo` and neither repair stage reaches the fault] → Not covered; the Node stays in `Demo` without radio. Accepted with the user's choice against a reboot in `Demo`.
- [The Mainframe is switched off for more than 5 minutes during a show] → Nodes reboot once into `Demo` and play the demo scene instead of the last show scene until the Mainframe sends again. Accepted by the user.
- [A Wi-Fi restart blocks the main loop and the strip output stutters] → Measure the duration of each stage on the device and watch the strip; fallback as described under stage 2.
- [Without a Mainframe the Node restarts the Wi-Fi driver every 30 s for hours, also in standalone `Demo`] → Accepted by the user. Extra power is estimated below 1 % of the radio's own draw (one repair of well under a second per 30 s, radio always receiving anyway); not measured. The device test runs this for at least ten attempts and checks output and free heap for a leak.
- [BUG-014 cannot be provoked] → The device test can only prove that both stages leave a working receive path and do not disturb the show. Whether they repair the real fault is known only when it occurs with the logger attached. bugs.md says so.
- [Repair runs while a blocking output delay is in progress (BUG-002)] → Repairs run from the main loop between output calls, so they only add to an existing delay.

## Device measurements

M5Atom Node (RGB strip, role wash), `node` build of `1eafc08`, serial log `logs/node-link-261117.log`, 2026-11-17. The Mainframe was unplugged at about 12:14:27 with the show running and plugged in again at about 12:22.

- ESP-NOW re-initialisation: 2–3 ms (two samples), `result=ok`.
- Wi-Fi driver restart: 20–22 ms (twelve samples), all `result=ok`.
- Free heap: 219584 bytes after the first stage, 219480 bytes after every Wi-Fi driver restart, unchanged over eight restarts in a row. No leak visible.
- Channel before every repair: 1.
- Repairs came at 30 s, 60 s, ... 270 s of silence; at 300 s `action=reboot`. The Node was back 1 s later with `BOOT: reset_reason=3` (software reset), kind rgb and role wash, in `Demo`.
- In `Demo` after the reboot: repairs again at 30 s to 150 s. When the Mainframe came back the Node processed the hello (`MAINFRAME: waiting_for_clock`) and printed `packets_back silent=166s attempts=5`. The receive path therefore works after five Wi-Fi driver restarts.
- Strip output: the user watched the strip during the repairs and saw no stutter.
- Not measured yet: packets returning after the ESP-NOW re-initialisation alone, a silence in `Demo` longer than the reboot period, and the Node taking scene and clock after a repair.
