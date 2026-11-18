# message-panel Specification

## Purpose

The message panel of the Mainframe display: the single surface for product text. The panel shows two kinds of content: texts, short reactions to motion events, and depression poems. This spec covers which events produce them, how text is laid out in lines with German letters and a curated emoji set, how long a message stays, which message wins when two compete, and how the panel is drawn over the eye. The eye itself is specified separately. Sources: `apps/mainframe/src/message_panel.{h,cpp}`, `apps/mainframe/src/panel_glyphs.{h,cpp}`, `drawMessagePanel` and `drawPanelLine` in `apps/mainframe/src/eye_renderer.cpp`, `serviceDepressionPoetry` and the panel calls in `apps/mainframe/src/main.cpp`, and `tests/message_panel.cpp`.

## Requirements

### Requirement: Single product text surface

The Mainframe SHALL show product text only through the message panel. Showing a message SHALL accept any non-empty UTF-8 text, store it up to a fixed byte capacity and make it the current message at once, replacing any message still visible. A model text SHALL become the current message when its emoji arrives and SHALL grow word by word up to the same capacity. There SHALL be no queue: a replaced message is dropped. An empty text SHALL be rejected and SHALL leave the current message unchanged.

#### Scenario: New message while another is visible
- **WHEN** a message is visible and another non-empty text is shown
- **THEN** the new text replaces the old one immediately and its display time starts once it is complete

#### Scenario: Empty text
- **WHEN** an empty text is passed to the panel
- **THEN** the panel rejects it and keeps the current message and its display time

### Requirement: Content sources

The V2 Mainframe SHALL feed the panel from exactly three sources: motion events from the IMU classifier, presence events and depression poems. Texts for motion and presence events SHALL come from the text model or from the text bank. The mood SHALL shape a model text but SHALL NOT trigger a text by itself; the button, the radio and the tempo path SHALL NOT produce panel text. The Mainframe SHALL NOT forward panel text to Nodes.

#### Scenario: Motion event
- **WHEN** the motion classifier reports an event other than None
- **THEN** the Mainframe first updates the personality with it and then offers it to the panel to pick a text

#### Scenario: Presence event
- **WHEN** a presence event is reported
- **THEN** the Mainframe first updates the personality with it and then offers it to the panel to pick a text

#### Scenario: High annoyance alone
- **WHEN** annoyance rises without a motion or presence event
- **THEN** the panel shows no text because of it

### Requirement: Display time

A message SHALL stay visible for a time proportional to the number of elements it occupies in the line flow, counting text lines and a leading emoji, but never shorter than a minimum time. The display time SHALL start when the message is complete: at once for a text bank line or poem, after the last word for a model text. A message that scrolls SHALL get a scrolling extra time on top, so that its end stays readable after the start delay. After that it SHALL disappear even if its triggering event persists. The panel SHALL have no other way to close a message. The times are under Current values.

#### Scenario: One short line
- **WHEN** the text `HELLO` is shown
- **THEN** it occupies one line and stays visible for the minimum time

#### Scenario: Five lines
- **WHEN** a text that wraps into five lines without an emoji is shown
- **THEN** it scrolls and stays visible for five per-element times plus the scrolling extra time, and is hidden afterwards

#### Scenario: Growing model text
- **WHEN** a model text needs one second to grow from its emoji to its last word
- **THEN** its display time starts after that second, not when the emoji appeared

### Requirement: Texts per motion kind

The panel SHALL take the text for a motion event from the text bank's lines for that motion kind. MultiTap SHALL use the double-tap lines for two taps and the three-or-more lines for three or more taps. Reposition SHALL produce a text although it changes no mood value. A bank line is stored with its emoji first; the panel SHALL show it with the emoji as its leading element and the text below, as for every other message.

#### Scenario: Triple tap
- **WHEN** a MultiTap event with a count of 3 reaches the panel outside its cooldown
- **THEN** the panel shows a line of the three-or-more lines of the text bank

#### Scenario: Double tap
- **WHEN** a MultiTap event with a count of 2 reaches the panel outside its cooldown
- **THEN** the panel shows a line of the double-tap lines of the text bank

### Requirement: Text selection from the bank

When the panel takes a text for a motion or presence event, it SHALL ask the text bank for a line of that event, with the lead mood value of the mood snapshot taken after the personality has applied the event.

#### Scenario: Annoyed tap
- **WHEN** a Tap raises annoyance to 90 and every other value lies between 40 and 60
- **THEN** the panel shows a Tap line with lead annoyance high

#### Scenario: Two taps in a row
- **WHEN** two Taps with the same lead value are answered one cooldown apart
- **THEN** the panel shows two different Tap lines

### Requirement: Text cooldown per motion kind

After the panel accepts an event for a text, it SHALL ignore further events of the same motion kind until a cooldown has passed, whether the text comes from the text model or from the text bank. The cooldown SHALL start when the panel accepts the event and SHALL be kept per motion kind, so an event of another kind SHALL be accepted at once. Double tap and multi tap SHALL share the MultiTap cooldown. An ignored event SHALL leave the current message, a growing model text and the cooldown unchanged.

#### Scenario: Repeated tap within the cooldown
- **WHEN** a Tap was accepted and another Tap arrives before the cooldown has passed
- **THEN** the panel ignores the second Tap

#### Scenario: Impact during a tap cooldown
- **WHEN** a MultiTap text is visible and an Impact arrives within the MultiTap cooldown
- **THEN** the panel accepts the Impact at once and its text replaces the MultiTap text

### Requirement: Depression poems

While depression is above a threshold, the Mainframe SHALL show a poem after each poem interval. The interval SHALL start when depression first exceeds the threshold. A due poem SHALL wait while any message is visible or a model text is requested, and appear once the panel is free; the next interval SHALL start when it appears. Falling to or below the threshold SHALL cancel the pending poem.

#### Scenario: Depression stays high
- **WHEN** depression exceeds the threshold and stays above it for one poem interval while the panel is free
- **THEN** the panel shows the next poem, and the following poem is due one interval later

#### Scenario: Panel busy when a poem is due
- **WHEN** a poem is due while a text is visible
- **THEN** the poem waits and appears in the first loop pass after the text has closed

#### Scenario: Model text requested when a poem is due
- **WHEN** a poem is due while the panel waits for a model text's emoji
- **THEN** the poem waits until that text, or the text bank line replacing it, has closed

#### Scenario: Depression drops
- **WHEN** depression falls to or below the threshold before the poem is due
- **THEN** no poem appears, and a new full interval starts only when depression exceeds the threshold again

### Requirement: Poem selection

The Mainframe SHALL take poems from one fixed list of German one-line poems, each ending in a curated emoji, in list order, and wrap to the first poem after the last. The position SHALL be kept in an 8-bit counter that advances with every poem shown and is not reset by a drop in depression. The poems are listed under Current values.

#### Scenario: First poems after boot
- **WHEN** the first two poems after boot are shown
- **THEN** they are `ICH WEINE IN DEINE HAND. 🪦` followed by `JEDER HAT DEN TOD IN SICH, WIE DIE FRUCHT DEN KERN. ⚰️`

### Requirement: Message priority

Texts SHALL take precedence over poems: a text that passes its cooldown SHALL replace any visible message, including a poem or another text, while a poem SHALL never replace a visible message. There SHALL be no further priority levels.

#### Scenario: Motion during a poem
- **WHEN** a poem is visible and a Shake event arrives outside the Shake cooldown
- **THEN** the panel replaces the poem with a Shake text

### Requirement: Line layout

The panel SHALL lay text out in left-aligned lines of at most eight columns. ASCII characters and the German letters Ä, Ö, Ü, ä, ö, ü and ß SHALL take one column, curated emoji two. Lines SHALL break at spaces, and spaces at a line break SHALL be dropped. A word wider than a line SHALL be split at the column limit. No break SHALL split a UTF-8 sequence or a curated emoji.

#### Scenario: Word wrap
- **WHEN** the text `NEXT MESSAGE` is shown
- **THEN** the first line is `NEXT` and the second `MESSAGE`

#### Scenario: Inline emoji
- **WHEN** the text `HÜHÜ 🙂 MUSIK 🎵` is shown
- **THEN** the first line is `HÜHÜ 🙂`, filling seven of eight columns

### Requirement: Vertical scrolling

The panel SHALL show at most four rows. If a message has more than four elements, it SHALL first stand still for a start delay and then move up one row per scroll step until its last four elements are visible, and stay there. A model text SHALL scroll in the same way, with the start delay counted from the arrival of its emoji: while it grows, new rows SHALL be added below the visible rows without moving the text, and the text SHALL never move further than its last four elements existing at that time. While elements remain below the visible rows, a small blinking triangle SHALL mark that more text follows. Delay, step and blink period are under Current values.

#### Scenario: Ten words
- **WHEN** a complete text of ten short words, one per line, is shown
- **THEN** rows 0 to 3 show its emoji and words 1 to 3 at first, after the start delay the text moves up one row per step, and it stops with words 7 to 10 visible

#### Scenario: End reached
- **WHEN** the last element of a long message is in the bottom visible row
- **THEN** the panel stops scrolling and the more-text triangle disappears

#### Scenario: Model text grows past four rows
- **WHEN** a model text's emoji and three text lines are visible and its next word starts a fourth text line before the start delay has passed
- **THEN** the panel keeps the emoji and the first three text lines in view, shows the more-text triangle, and moves up one row per scroll step once the start delay after the emoji has passed

#### Scenario: Model text still growing after the start delay
- **WHEN** a model text's start delay has passed and its last four elements are already visible
- **THEN** each new text line moves the text up so that the newest line is visible

### Requirement: Glyph vocabulary

The panel SHALL recognise a fixed, curated set of emoji, including multi-codepoint sequences, which SHALL match before their shorter prefixes. Any other non-ASCII code point SHALL take one column and be drawn as `?`. Umlauts SHALL be drawn as the base letter with two dots and ß as `B`. Curated emoji SHALL come from a 32-pixel sprite atlas: full size as the leading element, half size inline. The text model's emoji tokens SHALL be exactly this curated set.

#### Scenario: Multi-codepoint emoji
- **WHEN** the text contains the sequence for face with spiral eyes (😵 + ZWJ + 💫)
- **THEN** the panel treats it as one two-column glyph and not as 😵 followed by unknown characters

#### Scenario: Unknown emoji
- **WHEN** a message contains an emoji that is not in the curated set
- **THEN** the panel draws one `?` in a single column for each of its code points and does not treat it as a leading emoji

### Requirement: Drawing over the eye

The panel SHALL be drawn in the same frame after the eye, as an almost full-screen overlay: the eye keeps running beneath and stays visible through a scanline shade, a violet outline with cut corners frames the panel, and each text line and the leading emoji get their own dark block behind their content. The panel SHALL NOT replace the eye, also not for long text or while a model text grows.

#### Scenario: Message while the eye animates
- **WHEN** a message is visible
- **THEN** every rendered frame shows the current eye, darkened, with the panel and its text on top

### Requirement: Panel hidden while sleeping or in debug view

The renderer SHALL NOT draw the panel while the personality sleeps or while a debug screen (mood or presence) is shown. Message timing SHALL continue regardless: a message shown meanwhile counts as shown, starts its text cooldown or poem interval, and may expire unseen. While the panel is hidden, the panel SHALL NOT ask the text model and SHALL use the text bank.

#### Scenario: Poem during sleep
- **WHEN** a poem becomes due while the personality sleeps and the panel is free
- **THEN** the poem is taken from the list and the next interval starts, but nothing is drawn

#### Scenario: Debug overlay on
- **WHEN** a debug screen is shown and a text is triggered
- **THEN** the debug screen stays on screen without the panel, the text comes from the text bank without asking the model, and the message stays hidden unless debug mode is closed within its display time

#### Scenario: Tap while sleeping
- **WHEN** a Tap passes its cooldown while the personality sleeps
- **THEN** the panel takes a Tap line from the text bank without asking the model, and the text counts as shown although it is not drawn

### Requirement: Presence texts from the bank

The panel SHALL take the text for a presence event with text (`Kommt davor`, `Läuft vorbei`, `Geht weg`, `Steht weiter weg`) from the text bank's lines for that event, chosen as for motion texts. `Bewegt sich` SHALL show no text.

#### Scenario: Someone leaves while lonely
- **WHEN** `Geht weg` reaches the panel outside its cooldown and loneliness is the value farthest from the middle, at its high side
- **THEN** the panel shows a `Geht weg` line with lead loneliness high

#### Scenario: Someone moves
- **WHEN** `Bewegt sich` is reported
- **THEN** the panel shows nothing because of it

### Requirement: Presence texts equal to motion texts

Presence texts SHALL be treated exactly like motion texts: the same cooldown, kept per presence event kind, and the same priority. A presence text SHALL replace a visible poem or a visible motion or presence text of another kind, and a motion text SHALL replace a visible presence text the same way.

#### Scenario: Two visitors in a row
- **WHEN** a `Kommt davor` text was shown and another `Kommt davor` arrives before the cooldown has passed
- **THEN** the panel ignores the second one

#### Scenario: Gesture right after a presence text
- **WHEN** a `Kommt davor` text is visible and a Tap arrives outside the Tap cooldown
- **THEN** the panel replaces it with a Tap text at once

#### Scenario: Presence right after a gesture text
- **WHEN** a Tap text is visible and `Läuft vorbei` arrives outside its cooldown
- **THEN** the panel replaces it with a `Läuft vorbei` text at once

### Requirement: Model texts for reactions

For a motion event or a presence event with text that passes its cooldown while the panel is visible, the panel SHALL ask the text model for a text when the runtime is on. The panel SHALL show the emoji of the model text as soon as it arrives and then each finished word, so the message grows word by word. The previous message SHALL stay visible until that emoji arrives and SHALL then be replaced. Once the emoji is visible, the panel SHALL NOT switch to a text bank line for this event.

#### Scenario: Tap with the model running
- **WHEN** a Tap passes its cooldown while the panel is visible and the model runtime is on
- **THEN** the panel requests a text, shows the model's emoji when it arrives, and adds the words one by one until the text is complete

#### Scenario: Old message until the emoji
- **WHEN** a presence text is visible and an Impact passes its cooldown
- **THEN** the presence text stays until the Impact text's emoji arrives, and then the Impact text replaces it

### Requirement: Fallback to the text bank

The panel SHALL show a line from the text bank for the event, chosen as specified in Text selection from the bank with the mood snapshot of the event's acceptance, instead of a model text when the model runtime is off, when the panel is hidden, or when no emoji has arrived within the wait time after the panel accepted the event. In the last case the panel SHALL cancel the request. The wait time is in design.md.

#### Scenario: No model image
- **WHEN** a Shake passes its cooldown and the runtime reports no valid model image
- **THEN** the panel shows a Shake line from the text bank at once

#### Scenario: Emoji too late
- **WHEN** a Tap passes its cooldown and the model delivers no emoji within the wait time
- **THEN** the panel cancels the request and shows a Tap line from the text bank, chosen with the mood at the Tap's acceptance

### Requirement: Origin of a text visible to the owner

In a build with the text model runtime, the panel SHALL draw a text bank line with a different frame colour than a model text, so that the owner can tell at a glance where a text comes from. The mark SHALL NOT change the text or its emoji and SHALL be unobtrusive to an audience that does not know about it. The colours are in design.md Decision 7a.

#### Scenario: Text from the model
- **WHEN** the panel shows a text whose emoji came from the text model
- **THEN** the panel draws the normal frame

#### Scenario: Text bank line in a model build
- **WHEN** a model build shows a text bank line, such as a fallback after the wait time
- **THEN** the panel draws the text bank frame colour for as long as that text is visible

#### Scenario: Poem
- **WHEN** the panel shows a poem
- **THEN** the panel draws the normal frame

#### Scenario: Build without the runtime
- **WHEN** the normal build without the text model runtime shows any text
- **THEN** the panel draws the normal frame

### Requirement: Event number for the owner

In a build with the text model runtime, the panel SHALL show the number of the event of the current reaction text (text bank line or model text) small in white at the top left, in a small font independent of the text font, as a debug aid for the owner. The number SHALL be the event index of the text model request and of the `TEXT` serial lines.

#### Scenario: Number per event
- **WHEN** a model build shows the text for an event
- **THEN** the panel shows this number:

| Number | Event |
|---|---|
| 0 | Move |
| 1 | Tap |
| 2 | MultiTap 2 |
| 3 | MultiTap 3+ |
| 4 | Impact |
| 5 | Rotate |
| 6 | Reposition |
| 7 | Shake |
| 8 | Kommt davor |
| 9 | Läuft vorbei |
| 10 | Geht weg |
| 11 | Steht weiter weg |

#### Scenario: Knock in a model build
- **WHEN** a model build shows the text for a Tap
- **THEN** the panel shows the number 1 at the top left

#### Scenario: Poem
- **WHEN** the panel shows a poem
- **THEN** the panel shows no event number

#### Scenario: Build without the runtime
- **WHEN** the normal build without the text model runtime shows any text
- **THEN** the panel shows no event number

### Requirement: New event while a text is written

When an event passes its cooldown while a model text for an earlier event is still requested or growing, the panel SHALL cancel the earlier request and handle the new event as specified for model texts and fallback. The cancelled text SHALL NOT be completed.

#### Scenario: Shake while a Tap text grows
- **WHEN** a Tap text is growing and a Shake passes its cooldown
- **THEN** the Tap request is cancelled and the panel shows the Shake text's emoji when it arrives

### Requirement: Leading emoji

If a message ends with a curated emoji, or is a model text, the panel SHALL take that emoji out of the text lines, drop spaces before it, and show it as a large element centred in the first row, with the text lines below it. It SHALL count as one element for display time and scrolling and scroll with the text. An emoji inside the text SHALL stay inline. A German letter at the end SHALL stay in the text.

#### Scenario: Tap message
- **WHEN** the text `HUHU 🙂` is shown
- **THEN** row 0 shows the smile emoji large and centred and row 1 shows the text `HUHU`

#### Scenario: Emoji inside the text
- **WHEN** the text `HÜHÜ 🙂 MUSIK 🎵` is shown
- **THEN** the music emoji is shown large in row 0, and the smile stays inline in the first text line `HÜHÜ 🙂`

#### Scenario: Poem
- **WHEN** the poem `ICH WEINE IN DEINE HAND. 🪦` is shown
- **THEN** the grave emoji is shown large in row 0 and the poem's lines follow below it

## Current values

These are the values in the code on 2026-11-16 (reaction texts, emoji position and text model: 2026-11-18). They are calibration or product copy and may change without a spec change.

- Display time: 1 s per element (`kMessageDurationPerLineMs`), at least 4 s (`kMessageMinimumDurationMs`); scrolling messages get 2 s more (`kScrollingMessageExtraMs`).
- Text cooldown per motion kind and per presence event kind: 9 s (`kMotionCooldownMs`).
- Wait for the emoji of a model text: 2 s from the panel's acceptance of the event (`kModelEmojiWaitMs`); later, the text bank line is shown instead.
- Scroll start delay: 4 s until the first step (`kScrollStartMs`); scroll step: 0.8 s (`kScrollLineMs`); more-text triangle blink: 450 ms on, 450 ms off.
- Poem threshold: depression strictly above 50; poem interval: 120 s (`kDepressionPoemIntervalMs`, `main.cpp`).
- Text capacity: 192 bytes plus terminator (`kTextCapacity` = 193).
- Layout: 4 visible rows, 8 columns per line, font 1 at text size 2 (12 px per column), row pitch 22 px; inline emoji 16 px, leading emoji 32 px centred in row 0 (34 px slot; the text rows below start 12 px lower until the first scroll step).
- Curated emoji (47): 🙂 👋 👁 😵 🤢 ❗ 💢 💬 🎵 ↻ 🥰 😂 🫠 😍 😘 🥲 🤗 🫢 🙂‍↔️ 🥹 😢 😭 😡 💀 💩 🙈 👌 👉👈 🙌 😳 🙄 🫩 😌 😔 😵‍💫 🥺 😤 🤬 ☠️ 🖤 🔪 🥃 🌚 ⚰️ 🚬 🪦 ⚱️.
- Reaction texts: the text bank, generated from the corpus in `apps/mainframe/text_model/corpus/` by `apps/mainframe/tools/text_bank.py` into `apps/mainframe/src/text_bank_data.h`; 3,632 lines on 2026-11-18, about 30 per event and lead value. The 32 former fixed texts are lines of the corpus; their lead values are in `apps/mainframe/text_model/plan/fixed.tsv`. `Bewegt sich` shows no text. The former fixed Shake text is the corpus line `AAH, ICH KOTZE! 🤢` (before: `AAH - ICH KOTZE! 🤢`).
- Poems, in order:
  1. `ICH WEINE IN DEINE HAND. 🪦`
  2. `JEDER HAT DEN TOD IN SICH, WIE DIE FRUCHT DEN KERN. ⚰️`
  3. `DIE NACHT LEGT BLUMEN AN MEIN GRAB. ⚱️`
  4. `MEIN SCHATTEN ZÄHLT DIE LETZTEN KERZEN. 🚬`
  5. `UNTER DEM MOND WIRD SELBST DAS SCHWEIGEN SCHWER. 🌚`
  6. `DER ABEND SINKT INS KALTE GRAS. 🪦`
  7. `DIE ERDE HAT IHR LICHT VERLOREN. 🌚`
  8. `MEIN SPIEGELBILD SCHLIESST DIE AUGEN NICHT. 🚬`
  9. `IM LEEREN GLAS WIRD DAS LICHT SCHWER. 🥃`
  10. `DEINE LETZTE NACHRICHT IST BEREITS KALT. 🖤`
  11. `ICH HABE WIEDER NICHT ZURÜCKGERUFEN. 🫠`
