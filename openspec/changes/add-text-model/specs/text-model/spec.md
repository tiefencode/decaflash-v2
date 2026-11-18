# Spec Delta

## Purpose

The text model: our own small generative model that writes a new reaction text on the device from an event and the mood levels. Terms are those of the README section "Text model and text bank (host)".

## ADDED Requirements

### Requirement: Runtime only with a valid model image

The Mainframe SHALL start the text model only if the model partition holds an image whose header has the expected magic, format version and sizes, and the working memory can be allocated. Otherwise the runtime SHALL stay off, report the reason on the serial line once, and the show SHALL run with the text bank.

#### Scenario: Erased model partition
- **WHEN** the Mainframe boots with an empty model partition
- **THEN** the serial line reports that there is no model image, no model task computes, and the panel uses the text bank

#### Scenario: Wrong format version
- **WHEN** the image header names an unknown format version
- **THEN** the runtime stays off with that reason and the show runs normally

### Requirement: Request input

A request SHALL carry the event kind and the five mood values of the snapshot, each reduced to one of three levels (low, middle, high). The event kinds SHALL be the eight motion texts (Move, Tap, MultiTap with two taps, MultiTap with three or more, Impact, Rotate, Reposition, Shake) and the four presence events with text. The level boundaries are in design.md.

#### Scenario: Levels of a snapshot
- **WHEN** a Tap is requested with energy 10, annoyance 70, attention 50, loneliness 0 and depression 34
- **THEN** the request carries Tap, energy low, annoyance high, attention middle, loneliness low and depression middle

### Requirement: Compute only on request

The text model SHALL compute only while a request is open and SHALL otherwise not run. It SHALL compute on core 0 at a priority below the radio stack and SHALL yield regularly, so the idle task and the radio keep running.

#### Scenario: No events
- **WHEN** no motion or presence event asks for a text for ten minutes
- **THEN** the model performs no forward pass in that time

### Requirement: Output form

Every model text SHALL start with exactly one emoji from the panel's curated set, followed by text made only of the model's text alphabet: space, capital letters A–Z, Ä, Ö, Ü, digits and the punctuation listed in design.md, with no emoji. A text SHALL end at the model's end token or at the hard limit of output tokens, whichever comes first.

#### Scenario: First output token
- **WHEN** the model computes the first output token of a request
- **THEN** it can only choose one of the curated emoji

#### Scenario: Text too long
- **WHEN** the model has not produced its end token at the hard limit of output tokens
- **THEN** the text ends there

### Requirement: Streamed delivery

The text model SHALL hand the emoji to the panel as soon as it is chosen and each word as soon as the following space or the end of the text is produced, without waiting for the whole text.

#### Scenario: Emoji before words
- **WHEN** the model has chosen the emoji and is still computing the first word
- **THEN** the panel can already show the emoji

### Requirement: Cancel

A cancelled request SHALL stop within one forward pass and deliver nothing further. A new request SHALL cancel the open one.

#### Scenario: New request while writing
- **WHEN** a request is open and a new request arrives
- **THEN** the old request stops after at most one more forward pass and the model starts the new one

### Requirement: Varied texts

The model SHALL choose its output tokens by sampling with a seed that differs from request to request, so that the same event and mood levels can give different texts. With a fixed seed the same image, request and seed SHALL give the same text on the host and on the device.
The emoji and the first text token SHALL be sampled from a wider distribution than every later token; the temperature and top-k of both parts are the calibration values in design.md.

#### Scenario: Same tap twice
- **WHEN** two Tap requests with the same mood levels are computed with different seeds
- **THEN** the texts may differ

#### Scenario: Wide opening, narrow rest
- **WHEN** the model writes a text
- **THEN** the emoji and the first text token are sampled with the opening values and every later token with the narrower values

#### Scenario: Host equals device
- **WHEN** the host kernel and the device compute the same request with the same image and seed
- **THEN** they produce the same output tokens

### Requirement: Mouth only

The text model SHALL only produce panel text. It SHALL NOT change the mood, the Node light directive, the sounds, the show clock, the tempo path or anything sent over the radio, and it SHALL use no access point, internet or external service.

#### Scenario: Show without model
- **WHEN** the same music and the same events reach a Mainframe with and without a model image
- **THEN** mood values, light directives and show clock follow the same rules; only the panel texts differ
