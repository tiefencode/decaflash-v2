#include "text_model_form.h"

#include <cassert>
#include <cstdio>
#include <string>

#include "message_panel.h"

using namespace decaflash::mainframe;
using namespace decaflash::mainframe::text_model;

int main() {
  // Today's fixed reaction texts, in the model's order, all pass.
  const uint8_t count = fixedReactionTextCount();
  assert(count == 32);
  std::set<std::string> training;
  for (uint8_t i = 0; i < count; ++i) {
    const std::string text = fromPanelText(fixedReactionText(i));
    const FormResult result = checkForm(text);
    if (result != FormResult::Ok) {
      std::printf("fixed text %u fails (%s): %s\n", i, formResultName(result), text.c_str());
    }
    assert(result == FormResult::Ok);
    training.insert(wordsOf(text));
  }
  assert(fromPanelText(u8"HUHU 🙂") == u8"🙂 HUHU");
  assert(checkForm(u8"MOVE\tE_LOW\t🙂 HUHU") == FormResult::Ok);  // tags are skipped
  assert(checkForm(u8"🙂‍↔️ NICHT SO SCHÜTTELN") == FormResult::Ok);  // multi-codepoint emoji

  // Hand-made bad examples, all fail.
  assert(checkForm("") == FormResult::Empty);
  assert(checkForm(u8"🙂 huhu") == FormResult::UnknownCharacter);     // lower case
  assert(checkForm(u8"🙂 GRÜß DICH") == FormResult::UnknownCharacter);  // sharp s
  assert(checkForm(u8"🦄 HUHU") == FormResult::UnknownCharacter);     // not curated
  assert(checkForm(u8"🙂 HUHU; DU") == FormResult::UnknownCharacter);  // not in the alphabet
  assert(checkForm(u8"HUHU 🙂") == FormResult::NoLeadingEmoji);
  assert(checkForm(u8"🙂 HUHU 🎵") == FormResult::EmojiInText);
  assert(checkForm(u8"🙂") == FormResult::NoWords);
  assert(checkForm(u8"🙂   ") == FormResult::NoWords);
  assert(checkForm(u8"🙂 FINSCHASSEL") == FormResult::UnknownWord);  // not a corpus word
  assert(checkForm(u8"🙂 KLOPF , KLOPF") == FormResult::Spacing);
  assert(checkForm(u8"🙂 KLOPF- KLOPF") == FormResult::Spacing);
  assert(checkForm(u8"🙂  KLOPF") == FormResult::Spacing);
  assert(checkForm(u8"🙂 KLOPF ") == FormResult::Spacing);
  assert(checkForm(u8"🙂 KLOPF-KLOPF, KLOPF!") == FormResult::Ok);
  assert(checkForm(u8"🙂 GEHT'S?") == FormResult::Ok);  // the apostrophe is part of the word
  // 47 body characters pass, 48 do not; umlauts count as one character.
  std::string longest = u8"🙂 KLOPF";
  while (characterCount(longest) - 2 + 6 <= kMaxTextCharacters) longest += " KLOPF";
  while (characterCount(longest) - 2 < kMaxTextCharacters) longest += "!";
  assert(characterCount(wordsOf(longest)) == kMaxTextCharacters);
  assert(checkForm(longest) == FormResult::Ok);
  assert(checkForm(longest + "!") == FormResult::TooLong);
  assert(characterCount(u8"SCHÜTTELN") == 9);
  assert(checkForm(u8"🙄 KLOPF", training) == FormResult::Copy);  // other emoji, same words
  assert(checkForm(u8"💬 KLOPF KLOPF KLOPF", training) == FormResult::Ok);

  std::printf("PASS: text model form check: %u fixed texts pass, bad examples fail\n", count);
}
