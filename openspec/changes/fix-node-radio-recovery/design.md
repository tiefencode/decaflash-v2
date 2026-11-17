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
- A reboot stage, any Mainframe change, any Node transmission (see proposal.md).
- Recovery for a Node whose radio failed at boot and never received a packet.

## Decisions

### Silence watchdog as a hardware-independent class

`apps/node/src/radio_watchdog.h`, header only, no Arduino or ESP-IDF includes. It takes `onPacket(now)` and `poll(now)` and returns `None`, `ReinitEspNow` or `RestartWifi`. It is unarmed until the first `onPacket()`. After a silence period without `onPacket()` it returns `ReinitEspNow` once, then `RestartWifi` once per further period. `onPacket()` resets the sequence and reports, on the next `poll()`, that packets returned and after how many attempts. All time arithmetic is unsigned subtraction, so a `millis()` overflow is harmless.

Alternative considered: put the timing into `main.cpp`. Rejected: `main.cpp` is already too large (ideas.md) and the logic would not be host-testable.

### Values

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

### Serial output

- Per repair: `RADIO: silent=<s>s attempt=<n> channel=<c> action=espnow_reinit|wifi_restart result=ok|failed` followed, on failure, by the error codes of the steps.
- On return: `RADIO: packets_back silent=<s>s attempts=<n>`.
- `status`: `RADIO: ready=<0|1> channel=<c> last_packet_age=<s>s|never repairs=<n>`.

The channel comes from `esp_wifi_get_channel()` and is read before the repair, so a drifted channel is visible.

## Risks / Trade-offs

- [The fault of BUG-014 sits where neither stage reaches] → The log then shows repairs without `packets_back` while the Mainframe runs. BUG-014 stays open and the reboot stage is offered to the user again.
- [A Wi-Fi restart blocks the main loop and the strip output stutters] → Measure the duration of each stage on the device and watch the strip; fallback as described under stage 2.
- [A switched-off Mainframe makes the Node restart Wi-Fi every 30 s for hours] → Accepted by the user. The device test runs this for at least ten attempts and checks output and free heap for a leak.
- [BUG-014 cannot be provoked] → The device test can only prove that both stages leave a working receive path and do not disturb the show. Whether they repair the real fault is known only when it occurs with the logger attached. bugs.md says so.
- [Repair runs while a blocking output delay is in progress (BUG-002)] → Repairs run from the main loop between output calls, so they only add to an existing delay.
