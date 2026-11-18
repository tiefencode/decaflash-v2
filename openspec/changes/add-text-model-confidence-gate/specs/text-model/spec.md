## ADDED Requirements

### Requirement: Confidence per token

The text model SHALL deliver with each emoji, word and end of text a confidence value for every token it chose for that piece: the difference between the two highest log-probabilities of the sampling distribution (top-k at the sampling temperature) at that step. A higher value means a clearer choice. The value SHALL NOT change which token is sampled.

#### Scenario: Value with each piece
- **WHEN** the model hands a word to the panel
- **THEN** the piece carries the confidence values of the tokens that formed it, in order

#### Scenario: Sampling unchanged
- **WHEN** the same request with the same seed runs with and without reading the confidence values
- **THEN** the model writes the same text
