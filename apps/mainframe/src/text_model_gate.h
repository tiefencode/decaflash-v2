// Confidence gate (add-text-model-confidence-gate, design.md Decisions 1-3):
// the running mean of the per-token margins of a model text, emoji and end
// of text counted, compared with the threshold after every token from the
// check token on. Shared by the panel path (reaction_texts.cpp), the model
// task's TEXT line and the host tool text_model_write, so all three decide the
// same for the same seed.
#pragma once

namespace decaflash::mainframe {

// The panel acts on the gate only with DECAFLASH_TEXT_GATE=1. Switched off on
// 2026-11-20 at the user's request: the gate still decides and the TEXT line
// still reports gate=rejected, but no model text is replaced.
#ifndef DECAFLASH_TEXT_GATE
#define DECAFLASH_TEXT_GATE 0
#endif
constexpr bool kGateActsOnPanel = DECAFLASH_TEXT_GATE != 0;

constexpr int kGateCheckToken = 5;
// About 20 % of texts cancelled with the image w6x128-pre-ft-400-3e-3 and the
// sampling schedule (design.md Decision 3, measured on 1,200 host cases).
constexpr float kGateThreshold = 1.00f;

class ConfidenceGate {
 public:
  void reset() {
    sum_ = 0.0f;
    tokens_ = 0;
    cancelledAt_ = 0;
  }
  // Adds one token's margin. True exactly once: at the token that cancels.
  bool add(float margin) {
    if (cancelledAt_) return false;
    sum_ += margin;
    ++tokens_;
    if (tokens_ >= kGateCheckToken && mean() < kGateThreshold) {
      cancelledAt_ = tokens_;
      return true;
    }
    return false;
  }
  bool cancelled() const { return cancelledAt_ != 0; }
  int cancelledAt() const { return cancelledAt_; }
  int tokens() const { return tokens_; }
  float mean() const { return tokens_ ? sum_ / static_cast<float>(tokens_) : 0.0f; }

 private:
  float sum_ = 0.0f;
  int tokens_ = 0;
  int cancelledAt_ = 0;
};

}  // namespace decaflash::mainframe
