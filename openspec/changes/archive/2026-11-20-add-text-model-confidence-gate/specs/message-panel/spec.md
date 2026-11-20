## ADDED Requirements

### Requirement: Cancel of an unsure model text

While a model text grows, the panel SHALL keep the running mean of the confidence values of all tokens so far, the emoji counted as the first token and the end of text as the last. From the check token on, the panel SHALL compare the mean with the threshold after every token and SHALL cancel the request when the mean is below it. A text that ends before the check token SHALL NOT be checked. The check token and the threshold are in design.md. The cancel SHALL be active only in a build with `DECAFLASH_TEXT_GATE=1`; by default (`DECAFLASH_TEXT_GATE=0`) the panel SHALL NOT cancel for low confidence and model texts complete as before.

#### Scenario: Confidence drops while the text grows
- **WHEN** a model text for a Tap has reached the check token and the running mean falls below the threshold
- **THEN** the panel cancels the request, the growing text and its emoji disappear, and a Tap line from the text bank appears whole with the text bank frame colour

#### Scenario: Gate off by default
- **WHEN** the firmware is built without `DECAFLASH_TEXT_GATE=1`
- **THEN** no model text is cancelled for low confidence, whatever the running mean

#### Scenario: Confident text
- **WHEN** the running mean stays at or above the threshold until the end of text
- **THEN** the model text completes as before

#### Scenario: Short text
- **WHEN** a model text ends before the check token
- **THEN** the panel completes it without a check

#### Scenario: Low confidence before the check token
- **WHEN** the running mean is below the threshold at a token before the check token and back above it from the check token on
- **THEN** the text is not cancelled

### Requirement: Replacement after a cancel for low confidence

After a cancel for low confidence, the panel SHALL remove the growing text together with its emoji and show at once a whole text bank line for the same event, chosen with the mood snapshot of the event's acceptance, with the text bank frame colour as specified in Origin of a text visible to the owner.

#### Scenario: Replacement line
- **WHEN** the panel cancels a growing Shake text for low confidence
- **THEN** the Shake text and its emoji disappear and a whole Shake line from the text bank appears in the same pass, with the text bank frame colour and its own emoji

### Requirement: Cancel report on the serial line

In a build with the text model runtime, the Mainframe SHALL print one serial line for every cancel of an unsure model text, with the time, the token number, the running mean, the threshold, the cancelled partial text and the text bank line shown instead.

#### Scenario: One line per cancel
- **WHEN** the panel cancels an unsure model text
- **THEN** exactly one cancel line appears on the serial line
