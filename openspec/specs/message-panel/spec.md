# message-panel Specification

## Purpose

The message panel of the Mainframe display: the single surface for product text. The panel shows two kinds of content: texts, short reactions to motion events, and depression poems. This spec covers which events produce them, how text is laid out in lines with German letters and a curated emoji set, how long a message stays, which message wins when two compete, and how the panel is drawn over the eye. The eye itself is specified separately. Sources: `apps/mainframe/src/message_panel.{h,cpp}`, `apps/mainframe/src/panel_glyphs.{h,cpp}`, `drawMessagePanel` and `drawPanelLine` in `apps/mainframe/src/eye_renderer.cpp`, `serviceDepressionPoetry` and the panel calls in `apps/mainframe/src/main.cpp`, and `tests/message_panel.cpp`.

## Requirements

### Requirement: Single product text surface

The Mainframe SHALL show product text only through the message panel. Showing a message SHALL accept any non-empty UTF-8 text, store it up to a fixed byte capacity and make it the current message at once, replacing any message still visible. There SHALL be no queue: a replaced message is dropped. An empty text SHALL be rejected and SHALL leave the current message unchanged.

#### Scenario: New message while another is visible
- **WHEN** a message is visible and another non-empty text is shown
- **THEN** the new text replaces the old one immediately and its display time starts from that moment

#### Scenario: Empty text
- **WHEN** an empty text is passed to the panel
- **THEN** the panel rejects it and keeps the current message and its display time

### Requirement: Content sources

The V2 Mainframe SHALL feed the panel from exactly three sources: motion events from the IMU classifier, presence events and depression poems. No other mood value, the button, the radio or the tempo path SHALL produce panel text. The Mainframe SHALL NOT forward panel text to Nodes.

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

A message SHALL stay visible for a time proportional to the number of elements it occupies in the line flow, counting text lines and a trailing emoji, but never shorter than a minimum time. A message that scrolls SHALL get a scrolling extra time on top, so that its end stays readable after the start delay. After that it SHALL disappear even if its triggering event persists. The panel SHALL have no other way to close a message. The times are under Current values.

#### Scenario: One short line
- **WHEN** the text `HELLO` is shown
- **THEN** it occupies one line and stays visible for the minimum time

#### Scenario: Five lines
- **WHEN** a text that wraps into five lines without a trailing emoji is shown
- **THEN** it scrolls and stays visible for five per-element times plus the scrolling extra time, and is hidden afterwards

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

After a text, the panel SHALL ignore further events of the same motion kind until a cooldown has passed. The cooldown SHALL be kept per motion kind, so an event of another kind SHALL show at once. Double tap and multi tap SHALL share the MultiTap cooldown. An ignored event SHALL leave the current message and the cooldown unchanged.

#### Scenario: Repeated tap within the cooldown
- **WHEN** a Tap text was shown and another Tap arrives before the cooldown has passed
- **THEN** the panel ignores the second Tap

#### Scenario: Impact during a tap cooldown
- **WHEN** a MultiTap text is visible and an Impact arrives within the MultiTap cooldown
- **THEN** the panel replaces it with an Impact text at once

### Requirement: Depression poems

While depression is above a threshold, the Mainframe SHALL show a poem after each poem interval. The interval SHALL start when depression first exceeds the threshold. A due poem SHALL wait while any message is visible and appear once the panel is free; the next interval SHALL start when it appears. Falling to or below the threshold SHALL cancel the pending poem.

#### Scenario: Depression stays high
- **WHEN** depression exceeds the threshold and stays above it for one poem interval while the panel is free
- **THEN** the panel shows the next poem, and the following poem is due one interval later

#### Scenario: Panel busy when a poem is due
- **WHEN** a poem is due while a text is visible
- **THEN** the poem waits and appears in the first loop pass after the text has closed

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

### Requirement: Trailing emoji

If the last character of a message is a curated emoji, the panel SHALL take it out of the text lines, drop spaces before it, and show it as a large element in its own row directly after the last text line. It SHALL count as one element for display time and scrolling and scroll with the text. A German letter at the end SHALL stay in the text.

#### Scenario: Tap message
- **WHEN** the text `HUHU 🙂` is shown
- **THEN** row 0 shows the text `HUHU` and row 1 shows the smile emoji large

#### Scenario: Emoji inside the text
- **WHEN** the text `HÜHÜ 🙂 MUSIK 🎵` is shown
- **THEN** the smile stays inline in the first line and only the music emoji becomes the trailing element

### Requirement: Vertical scrolling

The panel SHALL show at most four rows. If a message has more than four elements, it SHALL first stand still for a start delay and then move up one row per scroll step until its last four elements are visible, and stay there. While elements remain below the visible rows, a small blinking triangle SHALL mark that more text follows. Delay, step and blink period are under Current values.

#### Scenario: Ten words
- **WHEN** a text of ten short words, one per line, is shown
- **THEN** rows 0 to 3 show words 1 to 4 at first, after the start delay the text moves up one row per step, and it stops with words 7 to 10 visible

#### Scenario: End reached
- **WHEN** the last element of a long message is in the bottom visible row
- **THEN** the panel stops scrolling and the more-text triangle disappears

### Requirement: Glyph vocabulary

The panel SHALL recognise a fixed, curated set of emoji, including multi-codepoint sequences, which SHALL match before their shorter prefixes. Any other non-ASCII code point SHALL take one column and be drawn as `?`. Umlauts SHALL be drawn as the base letter with two dots and ß as `B`. Curated emoji SHALL come from a 32-pixel sprite atlas: full size as a trailing element, half size inline.

#### Scenario: Multi-codepoint emoji
- **WHEN** the text contains the sequence for face with spiral eyes (😵 + ZWJ + 💫)
- **THEN** the panel treats it as one two-column glyph and not as 😵 followed by unknown characters

#### Scenario: Unknown emoji
- **WHEN** a message contains an emoji that is not in the curated set
- **THEN** the panel draws one `?` in a single column for each of its code points and does not treat it as a trailing emoji

### Requirement: Drawing over the eye

The panel SHALL be drawn in the same frame after the eye, as an almost full-screen overlay: the eye keeps running beneath and stays visible through a scanline shade, a violet outline with cut corners frames the panel, and each text line and the trailing emoji get their own dark block behind their content. The panel SHALL NOT replace the eye, also not for long text.

#### Scenario: Message while the eye animates
- **WHEN** a message is visible
- **THEN** every rendered frame shows the current eye, darkened, with the panel and its text on top

### Requirement: Panel hidden while sleeping or in debug view

The renderer SHALL NOT draw the panel while the personality sleeps or while a debug screen (mood or presence) is shown. Message timing SHALL continue regardless: a message shown meanwhile counts as shown, starts its text cooldown or poem interval, and may expire unseen.

#### Scenario: Poem during sleep
- **WHEN** a poem becomes due while the personality sleeps and the panel is free
- **THEN** the poem is taken from the list and the next interval starts, but nothing is drawn

#### Scenario: Debug overlay on
- **WHEN** a debug screen is shown and a text is triggered
- **THEN** the debug screen stays on screen without the panel, and the message stays hidden unless debug mode is closed within its display time

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

## Current values

These are the values in the code on 2026-11-16 (reaction texts: 2026-11-18). They are calibration or product copy and may change without a spec change.

- Display time: 1 s per element (`kMessageDurationPerLineMs`), at least 4 s (`kMessageMinimumDurationMs`); scrolling messages get 2 s more (`kScrollingMessageExtraMs`).
- Text cooldown per motion kind and per presence event kind: 9 s (`kMotionCooldownMs`).
- Scroll start delay: 4 s until the first step (`kScrollStartMs`); scroll step: 0.8 s (`kScrollLineMs`); more-text triangle blink: 450 ms on, 450 ms off.
- Poem threshold: depression strictly above 50; poem interval: 120 s (`kDepressionPoemIntervalMs`, `main.cpp`).
- Text capacity: 192 bytes plus terminator (`kTextCapacity` = 193).
- Layout: 4 visible rows, 8 columns per line, font 1 at text size 2 (12 px per column), row pitch 22 px; inline emoji 16 px, trailing emoji 32 px.
- Curated emoji (47): 🙂 👋 👁 😵 🤢 ❗ 💢 💬 🎵 ↻ 🥰 😂 🫠 😍 😘 🥲 🤗 🫢 🙂‍↔️ 🥹 😢 😭 😡 💀 💩 🙈 👌 👉👈 🙌 😳 🙄 🫩 😌 😔 😵‍💫 🥺 😤 🤬 ☠️ 🖤 🔪 🥃 🌚 ⚰️ 🚬 🪦 ⚱️.
- Reaction texts: the text bank, generated from the corpus in `apps/mainframe/text_model/corpus/` by `apps/mainframe/tools/text_bank.py` into `apps/mainframe/src/text_bank_data.h`; 3,632 lines on 2026-11-18, about 30 per event and lead value. The 32 former fixed texts are lines of the corpus; their lead values are in `apps/mainframe/text_model/plan/fixed.tsv`. `Bewegt sich` shows no text.
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
