// Language model prototype (docs/slm-prototype.md), measurement builds only:
// the model as a background task on core 0. Its output goes nowhere; the main
// loop only reads counters for the loop-load report.
#pragma once

#ifndef DECAFLASH_SLM
#define DECAFLASH_SLM 0
#endif

#if DECAFLASH_SLM

#include <stdint.h>

namespace decaflash {
namespace mainframe {

struct SlmReport {
  const char* state;        // "running" or the reason the runtime is off
  uint32_t forwards;        // forward passes since the previous report
  uint32_t forwardAvgUs;    // wall time per forward pass, yields included
  uint32_t forwardMaxUs;
  uint32_t pieces;          // output pieces since start
  uint32_t runs;            // completed generations since start
  uint32_t mismatches;      // runs whose output differed from the host reference
  uint32_t stackFreeBytes;  // smallest free stack of the model task so far
};

// Maps the model partition, takes the working memory and starts the task.
// Leaves the runtime off, with the reason on the serial line, if anything is
// missing. Never blocks on the model afterwards.
void slmBegin();

// Counters for the report; resets the per-report part. Does not block.
SlmReport slmTakeReport();

}  // namespace mainframe
}  // namespace decaflash

#endif  // DECAFLASH_SLM
