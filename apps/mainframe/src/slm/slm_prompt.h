// Language model prototype (docs/slm-prototype.md): optional test block that a
// host tool appends directly after a model image. It carries a fixed prompt,
// the table from output class to input token, and the output classes the host
// produced, so a device can run the same input and compare. All values are
// little-endian uint32 and may be unaligned. Hardware independent.
//
//   magic "SLMP", version 1, bos input token, eos class,
//   prompt count, table count, expected count,
//   prompt tokens, table entries, expected classes
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace slm {

constexpr uint32_t kPromptMagic = 0x504D4C53u;  // "SLMP"
constexpr uint32_t kPromptVersion = 1;
constexpr uint32_t kPromptHeaderWords = 7;
constexpr uint32_t kPromptMaxCount = 4096;

class PromptBlock {
 public:
  // Returns false and stays empty if `available` bytes at `base` do not hold a
  // complete block. Reads nothing beyond `available`.
  bool parse(const uint8_t* base, size_t available) {
    valid_ = false;
    if (base == nullptr || available < kPromptHeaderWords * 4) return false;
    base_ = base;
    if (word(0) != kPromptMagic || word(1) != kPromptVersion) return false;
    promptCount_ = word(4);
    tableCount_ = word(5);
    expectedCount_ = word(6);
    if (promptCount_ > kPromptMaxCount || tableCount_ > kPromptMaxCount ||
        expectedCount_ > kPromptMaxCount) {
      return false;
    }
    const size_t words = kPromptHeaderWords + promptCount_ + tableCount_ + expectedCount_;
    if (available / 4 < words) return false;
    valid_ = true;
    return true;
  }

  bool valid() const { return valid_; }
  uint32_t bosInput() const { return word(2); }
  uint32_t eosClass() const { return word(3); }
  uint32_t promptCount() const { return promptCount_; }
  uint32_t tableCount() const { return tableCount_; }
  uint32_t expectedCount() const { return expectedCount_; }
  uint32_t prompt(uint32_t index) const { return word(kPromptHeaderWords + index); }
  uint32_t inputForClass(uint32_t outputClass) const {
    return word(kPromptHeaderWords + promptCount_ + outputClass);
  }
  uint32_t expected(uint32_t index) const {
    return word(kPromptHeaderWords + promptCount_ + tableCount_ + index);
  }

 private:
  uint32_t word(size_t index) const {
    uint32_t value;
    memcpy(&value, base_ + index * 4, 4);
    return value;
  }

  const uint8_t* base_ = nullptr;
  uint32_t promptCount_ = 0;
  uint32_t tableCount_ = 0;
  uint32_t expectedCount_ = 0;
  bool valid_ = false;
};

}  // namespace slm
