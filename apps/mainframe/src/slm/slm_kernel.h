// Language model prototype (docs/slm-prototype.md): hardware-independent
// wrapper around the reference inference code in llm.h. It checks a model
// image against the bytes actually available before anything is bound, and
// offers reset / step with greedy choice of the next output class. Memory
// comes from caller-supplied allocators, so the host test and the device
// share this file. Nothing here touches ESP-IDF or Arduino.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define LLM_INT8_ACT 1  // same arithmetic path as the reference firmware
#include "llm.h"

namespace slm {

enum class LoadResult : uint8_t {
  Ok,
  TooSmall,    // fewer bytes than a header
  BadMagic,    // not a model image (an erased partition lands here)
  BadVersion,  // unknown format version or flag
  BadHeader,   // dimensions the kernel cannot use
  Truncated,   // image needs more bytes than are available
};

inline const char* loadResultName(LoadResult result) {
  switch (result) {
    case LoadResult::Ok: return "ok";
    case LoadResult::TooSmall: return "too small";
    case LoadResult::BadMagic: return "no model image";
    case LoadResult::BadVersion: return "unknown format version";
    case LoadResult::BadHeader: return "invalid header";
    case LoadResult::Truncated: return "image truncated";
  }
  return "?";
}

namespace detail {

// Walks the tensor layout of llm_load with bounds checks. llm_load itself
// reads a group prefix per tensor and trusts the image to be complete.
class ImageWalker {
 public:
  ImageWalker(const uint8_t* base, size_t available)
      : base_(base), available_(available) {}

  void seek(uint64_t offset) { offset_ = offset; }
  uint64_t offset() const { return offset_; }
  bool ok() const { return ok_; }
  bool badGroup() const { return badGroup_; }

  void quant(uint64_t rows, uint64_t cols) {
    if (!ok_) return;
    if (offset_ + 4 > available_) { ok_ = false; return; }
    int32_t group;
    memcpy(&group, base_ + offset_, 4);
    if (group <= 0) { ok_ = false; badGroup_ = true; return; }
    const uint64_t groups = (cols + static_cast<uint64_t>(group) - 1) / static_cast<uint64_t>(group);
    skip(4 + rows * ((cols + 1) / 2) + rows * groups * 2);
  }

  void floats(uint64_t count) { skip(count * 4); }

 private:
  void skip(uint64_t bytes) {
    if (!ok_) return;
    if (bytes > available_ || offset_ + bytes > available_) { ok_ = false; return; }
    offset_ += bytes;
  }

  const uint8_t* base_;
  uint64_t available_;
  uint64_t offset_ = 0;
  bool ok_ = true;
  bool badGroup_ = false;
};

}  // namespace detail

// Checks that `available` bytes at `base` hold a complete image this kernel
// can run. Reads nothing beyond `available`.
inline LoadResult checkImage(const uint8_t* base, size_t available) {
  if (base == nullptr || available < LLM_HEADER_BYTES) return LoadResult::TooSmall;
  uint32_t head[6];
  memcpy(head, base, sizeof(head));  // magic, version, header bytes, flags, vocab in, vocab out
  int32_t dims[7];
  memcpy(dims, base + 24, sizeof(dims));  // dim, layers, heads, ffn, ple dim, seq len, group
  if (head[0] != LLM_MAGIC) return LoadResult::BadMagic;
  if (head[1] == 0 || head[1] > LLM_FORMAT_VERSION) return LoadResult::BadVersion;
  if (head[3] & ~static_cast<uint32_t>(LLM_FLAGS_KNOWN)) return LoadResult::BadVersion;
  if (head[2] < LLM_HEADER_BYTES || head[2] > LLM_HEADER_MAX) return LoadResult::BadHeader;
  const int64_t vocab = static_cast<int32_t>(head[4]);
  const int64_t outVocab = static_cast<int32_t>(head[5]);
  const int64_t dim = dims[0], layers = dims[1], heads = dims[2], ffn = dims[3];
  const int64_t pleDim = dims[4], seqLen = dims[5], group = dims[6];
  const bool tied = (head[3] & LLM_FLAG_TIED_HEAD) != 0;
  if (vocab <= 0 || outVocab <= 0 || dim <= 0 || ffn <= 0 || pleDim <= 0 || seqLen <= 0 ||
      group <= 0 || layers <= 0 || layers > LLM_MAX_LAYERS || heads <= 0 || dim % heads != 0 ||
      (dim / heads) % 2 != 0 || (tied && outVocab > vocab) || dim > LLM_Q8_MAX_INPUT ||
      ffn > LLM_Q8_MAX_INPUT || pleDim > LLM_Q8_MAX_INPUT) {
    return LoadResult::BadHeader;
  }

  detail::ImageWalker walk(base, available);
  walk.seek(head[2]);
  walk.quant(vocab, dim);
  walk.quant(layers * pleDim, dim);
  walk.floats(pleDim);
  walk.quant(vocab, layers * pleDim);
  for (int64_t layer = 0; layer < layers; ++layer) {
    walk.floats(dim);
    walk.quant(3 * dim, dim);
    walk.quant(dim, dim);
    walk.floats(dim);
    walk.quant(ffn, dim);
    walk.quant(ffn, dim);
    walk.quant(dim, ffn);
    walk.quant(pleDim, dim);
    walk.quant(dim, pleDim);
    walk.floats(dim);
  }
  walk.floats(dim);
  if (!tied) walk.quant(outVocab, dim);
  if (walk.badGroup()) return LoadResult::BadHeader;
  return walk.ok() ? LoadResult::Ok : LoadResult::Truncated;
}

using Allocator = void* (*)(size_t bytes);

class Kernel {
 public:
  // Binds the image in place. On any result but Ok the kernel stays unusable.
  LoadResult load(const uint8_t* base, size_t available) {
    ready_ = false;
    loaded_ = false;
    const LoadResult result = checkImage(base, available);
    if (result != LoadResult::Ok) return result;
    if (llm_load(base, &model_) != 0) return LoadResult::BadHeader;
    loaded_ = true;
    return LoadResult::Ok;
  }

  // Unpacks the layer weights into 8-bit copies from `weights` and takes the
  // working buffers: small hot ones from `scratch`, the KV cache from `cache`.
  // Returns false if any allocation fails; nothing is freed, the caller is
  // expected to leave the runtime off.
  bool allocate(Allocator weights, Allocator scratch, Allocator cache) {
    if (!loaded_) return false;
    const int dim = model_.c.dim, layers = model_.c.n_layers, pleDim = model_.c.ple_dim;
    const int ffn = model_.c.ffn, seqLen = model_.c.seq_len;
    if (llm_stage_core_int8_alloc(&model_, weights) != llm_core_stage_count(&model_)) return false;
    if (model_.out_head.codes != model_.tok_emb.codes) {  // untied head: read once per step
      void* buffer = weights(llm_stage_int8_bytes(&model_.out_head));
      if (buffer == nullptr) return false;
      llm_stage_int8(&model_.out_head, buffer);
    }
    const auto floats = [](Allocator alloc, size_t count) {
      return static_cast<float*>(alloc(count * sizeof(float)));
    };
    scratch_.x = floats(scratch, dim);
    scratch_.h = floats(scratch, ffn > dim ? ffn : dim);
    scratch_.qkv = floats(scratch, 3 * static_cast<size_t>(dim));
    scratch_.att = floats(scratch, dim);
    scratch_.g1 = floats(scratch, ffn);
    scratch_.g2 = floats(scratch, pleDim > ffn ? pleDim : ffn);
    scratch_.ple = floats(scratch, static_cast<size_t>(layers) * pleDim);
    scratch_.tmpP = floats(scratch, static_cast<size_t>(layers) * pleDim);
    scratch_.trow = floats(scratch, static_cast<size_t>(layers) * pleDim);
    scratch_.logits = floats(scratch, model_.out_vocab);
    scratch_.scores = floats(scratch, seqLen);
    const size_t cacheFloats = static_cast<size_t>(layers) * seqLen * dim;
    scratch_.kcache = floats(cache, cacheFloats);
    scratch_.vcache = floats(cache, cacheFloats);
    // The norm vectors follow byte-packed tensors in the image and may be
    // unaligned there; aligned copies let rmsnorm take its fast path, as the
    // reference firmware does.
    bool normsCopied = copyNorm(scratch, &model_.ple_proj_norm, pleDim) &&
                       copyNorm(scratch, &model_.out_norm, dim);
    for (int layer = 0; layer < layers; ++layer) {
      normsCopied = normsCopied && copyNorm(scratch, &model_.attn_norm[layer], dim) &&
                    copyNorm(scratch, &model_.ffn_norm[layer], dim) &&
                    copyNorm(scratch, &model_.ple_norm[layer], dim);
    }
    ready_ = normsCopied && scratch_.x && scratch_.h && scratch_.qkv && scratch_.att &&
             scratch_.g1 && scratch_.g2 && scratch_.ple && scratch_.tmpP && scratch_.trow &&
             scratch_.logits && scratch_.scores && scratch_.kcache && scratch_.vcache;
    position_ = 0;
    return ready_;
  }

  bool ready() const { return ready_; }
  int inputVocab() const { return model_.c.vocab; }
  int outputVocab() const { return model_.out_vocab; }
  int contextLength() const { return model_.c.seq_len; }
  int layers() const { return model_.c.n_layers; }
  int dimension() const { return model_.c.dim; }
  size_t imageBytes() const { return model_.image_bytes; }
  int position() const { return position_; }

  // Starts a new text; the KV cache is overwritten position by position.
  void reset() { position_ = 0; }

  // Feeds one input token and returns the most likely output class, or -1 if
  // the kernel is not ready, the token is out of range or the context is full.
  int step(int inputToken) {
    if (!ready_ || inputToken < 0 || inputToken >= model_.c.vocab ||
        position_ >= model_.c.seq_len) {
      return -1;
    }
    llm_forward(&model_, inputToken, position_++, &scratch_);
    int best = 0;
    for (int k = 1; k < model_.out_vocab; ++k) {
      if (scratch_.logits[k] > scratch_.logits[best]) best = k;
    }
    return best;
  }

  // Platform hook for a per-layer yield or a second core; see llm.h.
  void setLayerMatvec(void (*hook)(const QT*, const float*, float*)) { model_.layer_matvec = hook; }

 private:
  static bool copyNorm(Allocator alloc, const float** vector, int count) {
    void* copy = alloc(static_cast<size_t>(count) * sizeof(float));
    if (copy == nullptr) return false;
    memcpy(copy, *vector, static_cast<size_t>(count) * sizeof(float));
    *vector = static_cast<const float*>(copy);
    return true;
  }

  Model model_ = {};
  Scratch scratch_ = {};
  int position_ = 0;
  bool loaded_ = false;
  bool ready_ = false;
};

}  // namespace slm
