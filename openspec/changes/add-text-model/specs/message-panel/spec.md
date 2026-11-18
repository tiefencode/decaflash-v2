# Spec Delta

## ADDED Requirements

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

The panel SHALL show a subtle mark with every text from the text model and SHALL NOT show it with any other message, so that the owner can tell at a glance whether a text comes from the text model or from another source. The mark SHALL NOT change the text or its emoji, and it SHALL be unobtrusive enough that an audience that does not know about it is not drawn to it. The form of the mark is in design.md.

#### Scenario: Text from the model
- **WHEN** the panel shows a text whose emoji came from the text model
- **THEN** the panel shows the mark for as long as that text is visible

#### Scenario: Text from another source
- **WHEN** the panel shows a text that did not come from the text model, such as a fallback text or a poem
- **THEN** the panel shows no mark

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

## MODIFIED Requirements

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

### Requirement: Vertical scrolling

The panel SHALL show at most four rows. If a complete message has more than four elements, it SHALL first stand still for a start delay and then move up one row per scroll step until its last four elements are visible, and stay there. If a model text grows past four elements while it is written, the panel SHALL move up as each new row is added so that the newest word is visible, and SHALL then keep its last four elements in view. While elements remain below the visible rows, a small blinking triangle SHALL mark that more text follows. Delay, step and blink period are under Current values.

#### Scenario: Ten words
- **WHEN** a complete text of ten short words, one per line, is shown
- **THEN** rows 0 to 3 show its emoji and words 1 to 3 at first, after the start delay the text moves up one row per step, and it stops with words 7 to 10 visible

#### Scenario: End reached
- **WHEN** the last element of a long message is in the bottom visible row
- **THEN** the panel stops scrolling and the more-text triangle disappears

#### Scenario: Model text grows past four rows
- **WHEN** a model text's emoji and three text lines are visible and its next word starts a fourth text line
- **THEN** the panel moves up one row so that the new line is visible

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

### Requirement: Drawing over the eye

The panel SHALL be drawn in the same frame after the eye, as an almost full-screen overlay: the eye keeps running beneath and stays visible through a scanline shade, a violet outline with cut corners frames the panel, and each text line and the leading emoji get their own dark block behind their content. The panel SHALL NOT replace the eye, also not for long text or while a model text grows.

#### Scenario: Message while the eye animates
- **WHEN** a message is visible
- **THEN** every rendered frame shows the current eye, darkened, with the panel and its text on top

### Requirement: Glyph vocabulary

The panel SHALL recognise a fixed, curated set of emoji, including multi-codepoint sequences, which SHALL match before their shorter prefixes. Any other non-ASCII code point SHALL take one column and be drawn as `?`. Umlauts SHALL be drawn as the base letter with two dots and ß as `B`. Curated emoji SHALL come from a 32-pixel sprite atlas: full size as the leading element, half size inline. The text model's emoji tokens SHALL be exactly this curated set.

#### Scenario: Multi-codepoint emoji
- **WHEN** the text contains the sequence for face with spiral eyes (😵 + ZWJ + 💫)
- **THEN** the panel treats it as one two-column glyph and not as 😵 followed by unknown characters

#### Scenario: Unknown emoji
- **WHEN** a message contains an emoji that is not in the curated set
- **THEN** the panel draws one `?` in a single column for each of its code points and does not treat it as a leading emoji

## REMOVED Requirements

### Requirement: Trailing emoji

**Reason**: Replaced by "Leading emoji": the emoji of every message is now shown large in the first row, above the text (user decision, 2026-11-17), because the model writes the emoji first.

**Migration**: No data change. Text bank lines and poems keep their emoji at the end of the string; the panel lays it out in the first row.
