# Spec Delta

## MODIFIED Requirements

### Requirement: Locally synthesised sounds

The Mainframe SHALL synthesise every sound on the device from code and play it as raw PCM through the Voice Base speaker. It SHALL NOT load audio files and SHALL NOT need a network or external service to play a sound. The start sound SHALL be synthesised once during setup. The mood phrases and the snore SHALL each be synthesised once after boot, a small piece per main loop pass, and kept until reboot; playing one SHALL NOT synthesise it again. The Mainframe SHALL stop preparing sounds once the audio analysis is requested.

#### Scenario: Offline boot
- **WHEN** the Mainframe boots without any access point or internet connection
- **THEN** it plays the start sound

#### Scenario: Phrase played twice
- **WHEN** the same mood phrase is played a second time
- **THEN** the Mainframe plays the same prepared samples again without synthesising them

#### Scenario: Sounds prepared after boot
- **WHEN** the Mainframe has booted and the audio analysis is not requested
- **THEN** every main loop pass prepares a further piece until all phrases and the snore are ready

### Requirement: Mood phrases

An accepted crossing SHALL play the phrase for its mood and direction, whatever the threshold. There SHALL be nine phrases: UP and DOWN for energy, annoyance, attention and depression, and UP for loneliness. A phrase SHALL start the speaker, play once at the phrase volume and stop the speaker when its duration has passed. If the phrase is not prepared yet, or the speaker or playback fails, no sound SHALL play, the crossing SHALL NOT be replayed and the engine SHALL stay free.

#### Scenario: Same phrase for every threshold
- **WHEN** depression crosses two different thresholds upwards at different times
- **THEN** the same depression UP phrase plays both times

#### Scenario: Phrase ends
- **WHEN** a phrase's duration has passed
- **THEN** the Mainframe stops the speaker and the engine is free for the next sound

#### Scenario: Phrase not prepared yet
- **WHEN** a crossing is accepted shortly after boot and its phrase is not prepared yet
- **THEN** no phrase plays now or later for that crossing

### Requirement: Sleep snoring

While the personality sleeps and the analysis is not requested, the Mainframe SHALL play a snore whenever the personality's snore schedule is due and the start sound is finished with no phrase or snore playing. The first snore SHALL be due a pseudo-random delay after falling asleep, and each further one a new delay after the previous snore was started. Snores SHALL NOT be queued: one due time gives at most one snore. A snore that is due before the snore is prepared SHALL be dropped. The snore SHALL be quieter than the phrases.

#### Scenario: Falling asleep
- **WHEN** the personality falls asleep before the analysis is requested
- **THEN** no snore plays until the first snore delay has passed

#### Scenario: Next snore
- **WHEN** a snore has just played and the personality keeps sleeping
- **THEN** the next snore plays after a new delay from the snore schedule

#### Scenario: Waking up
- **WHEN** the personality wakes up
- **THEN** the snore schedule is cleared and no snore plays

#### Scenario: Analysis requested
- **WHEN** the analysis has been requested
- **THEN** the Mainframe plays no snore

### Requirement: Playback does not block the loop

Playback SHALL run in the background while the main loop continues with radio, show clock, mood and display. The end of every sound SHALL be detected from elapsed time in a later loop pass. Starting a phrase or snore SHALL NOT synthesise samples, and preparing the sounds SHALL take only a bounded, short time in each loop pass.

#### Scenario: Show during the start sound
- **WHEN** the show is started while the start sound is playing
- **THEN** scene and clock sending and the display continue while the start sound plays

#### Scenario: Phrase starts
- **WHEN** an accepted crossing starts its phrase
- **THEN** the loop pass that starts it reads the motion sensor again within the usual time, so a gesture in progress is kept
