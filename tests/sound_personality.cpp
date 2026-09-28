#include "sound_personality.h"

#include <cassert>
#include <iostream>

using namespace decaflash::mainframe;

int main() {
  ThresholdCrossingEvent event;
  Mood mood = {};

  MoodThresholdWatcher rising;
  mood.annoyance = 9;
  assert(!rising.update(mood, 0, true, event));
  mood.annoyance = 10;
  assert(rising.update(mood, 1, true, event));
  assert(event.state == SoundState::Annoyance && event.threshold == 10 &&
         event.direction == CrossingDirection::Up);

  MoodThresholdWatcher higher;
  mood = {};
  mood.annoyance = 49;
  assert(!higher.update(mood, 0, true, event));
  mood.annoyance = 50;
  assert(higher.update(mood, 1, true, event));
  assert(event.threshold == 50 && event.direction == CrossingDirection::Up);

  // A sound type's cooldown does not block a different mood type.
  MoodThresholdWatcher perMoodCooldown;
  mood = {};
  mood.annoyance = 9;
  mood.attention = 9;
  assert(!perMoodCooldown.update(mood, 0, true, event));
  mood.annoyance = 10;
  assert(perMoodCooldown.update(mood, 1, true, event));
  assert(event.state == SoundState::Annoyance);
  mood.attention = 10;
  assert(perMoodCooldown.update(mood, 2, true, event));
  assert(event.state == SoundState::Attention);
  mood.attention = 30;
  assert(!perMoodCooldown.update(mood, 3, true, event));

  // Falling voices exist only at 10, 30 and 50, not at 70 or 90.
  MoodThresholdWatcher falling;
  mood = {};
  mood.attention = 91;
  assert(!falling.update(mood, 0, true, event));
  mood.attention = 70;
  assert(!falling.update(mood, 1, true, event));
  mood.attention = 50;
  assert(falling.update(mood, sound_config::minIntervalMs + 1, true, event));
  assert(event.state == SoundState::Attention && event.threshold == 50 &&
         event.direction == CrossingDirection::Down);

  // Loneliness has no falling voice, even when it crosses a lower threshold.
  MoodThresholdWatcher lonely;
  mood = {};
  mood.loneliness = 31;
  assert(!lonely.update(mood, 0, true, event));
  mood.loneliness = 30;
  assert(!lonely.update(mood, 1, true, event));

  // Energy remains special: it only speaks at 10 down and 90 up.
  MoodThresholdWatcher energy;
  mood = {};
  mood.energy = 11;
  assert(!energy.update(mood, 0, true, event));
  mood.energy = 10;
  assert(energy.update(mood, 1, true, event));
  assert(event.state == SoundState::Energy && event.threshold == 10 &&
         event.direction == CrossingDirection::Down);
  mood.energy = 20;
  assert(!energy.update(mood, sound_config::minIntervalMs + 1, true, event));
  mood.energy = 89;
  assert(!energy.update(mood, sound_config::minIntervalMs * 2 + 1, true, event));
  mood.energy = 90;
  assert(energy.update(mood, sound_config::minIntervalMs * 3 + 1, true, event));
  assert(event.threshold == 90 && event.direction == CrossingDirection::Up);

  std::cout << "Sound personality tests passed\n";
}
