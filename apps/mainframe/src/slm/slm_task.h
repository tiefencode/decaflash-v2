// Text model runtime (add-text-model, task 7.2), model builds only: the
// kernel as a task on core 0 that writes one text per request and sleeps
// without one. The panel path sees it as a TextModel (reaction_texts.h).
#pragma once

#ifndef DECAFLASH_SLM
#define DECAFLASH_SLM 0
#endif

#if DECAFLASH_SLM

#include <stdint.h>

#include "../reaction_texts.h"

namespace decaflash {
namespace mainframe {

struct SlmReport {
  const char* state;        // "running" or the reason the runtime is off
  uint32_t forwards;        // forward passes since the previous report
  uint32_t forwardAvgUs;    // wall time per forward pass, yields included
  uint32_t forwardMaxUs;
  uint32_t pieces;          // delivered emoji and words since start
  uint32_t runs;            // finished requests since start (written or cancelled)
  uint32_t stackFreeBytes;  // smallest free stack of the model task so far
};

// Maps the model partition, takes the working memory and starts the task.
// Leaves the runtime off, with the reason on the serial line, if anything is
// missing. Never blocks on the model afterwards.
void slmBegin();

// The runtime as the panel path uses it; ready() is false until slmBegin()
// succeeded.
TextModel& slmTextModel();

// Counters for the report; resets the per-report part. Does not block.
SlmReport slmTakeReport();

}  // namespace mainframe
}  // namespace decaflash

#endif  // DECAFLASH_SLM
