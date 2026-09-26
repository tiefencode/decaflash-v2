#include "node_output.h"

namespace {

const char* rendererNameFor(decaflash::NodeKind nodeKind) {
  switch (nodeKind) {
    case decaflash::NodeKind::RgbStrip:
      return "rgb_strip";

    case decaflash::NodeKind::Flashlight:
    default:
      return "flash";
  }
}

}  // namespace

void NodeOutput::setNodeProfile(decaflash::NodeKind nodeKind, decaflash::NodeEffect nodeEffect) {
  const bool sameProfile =
    nodeKindInitialized_ && nodeKind_ == nodeKind && nodeEffect_ == nodeEffect;
  if (sameProfile) {
    return;
  }

  if (nodeKindInitialized_ && nodeKind_ != nodeKind) {
    allOff();
  }

  nodeKind_ = nodeKind;
  nodeEffect_ = nodeEffect;
  nodeKindInitialized_ = true;

  switch (nodeKind_) {
    case decaflash::NodeKind::RgbStrip:
      rgbStrip_.begin();
      rgbStrip_.setNodeEffect(nodeEffect_);
      break;

    case decaflash::NodeKind::Flashlight:
    default:
      flashlight_.begin();
      break;
  }
}

void NodeOutput::setFlashCommand(const decaflash::FlashCommand& command) {
  flashlight_.setCommand(command);
}

void NodeOutput::setRgbCommand(const decaflash::RgbCommand& command) {
  rgbStrip_.setNodeEffect(nodeEffect_);
  rgbStrip_.setCommand(command);
}

void NodeOutput::setVisualState(const decaflash::NodeVisualState& state) {
  visualState_ = state;
  rgbStrip_.setVisualState(state);
}

bool NodeOutput::hasVisualOverride() const {
  return visualState_.rgbMode != decaflash::RgbRenderMode::Scene ||
    visualState_.brightnessPercent != 100 ||
    visualState_.overlayOpacityPercent != 0 ||
    visualState_.flashOverride != decaflash::FlashOverride::Scene;
}

void NodeOutput::triggerRgbPulseRow() {
  if (hasVisualOverride()) return;
  if (nodeKind_ == decaflash::NodeKind::RgbStrip) {
    rgbStrip_.triggerPulseRow();
  }
}

void NodeOutput::syncBeatClock(
  uint32_t now,
  uint32_t beatIntervalMs,
  uint8_t beatsPerBar,
  uint8_t beatInBar,
  uint32_t currentBar
) {
  if (nodeKind_ == decaflash::NodeKind::RgbStrip) {
    rgbStrip_.syncBeatClock(now, beatIntervalMs, beatsPerBar, beatInBar, currentBar);
  }
}

void NodeOutput::allOff() {
  if (!nodeKindInitialized_) {
    return;
  }

  switch (nodeKind_) {
    case decaflash::NodeKind::RgbStrip:
      rgbStrip_.allOff();
      break;

    case decaflash::NodeKind::Flashlight:
    default:
      flashlight_.allOff();
      break;
  }
}

void NodeOutput::flash100(uint16_t flashMs) {
  if (hasVisualOverride()) return;
  switch (nodeKind_) {
    case decaflash::NodeKind::RgbStrip:
      rgbStrip_.flash100(flashMs);
      break;

    case decaflash::NodeKind::Flashlight:
    default:
      flashlight_.flash100(flashMs);
      break;
  }
}

void NodeOutput::showTemporaryLit(bool lit) {
  if (hasVisualOverride()) return;
  switch (nodeKind_) {
    case decaflash::NodeKind::RgbStrip:
      rgbStrip_.setLit(lit);
      break;

    case decaflash::NodeKind::Flashlight:
    default:
      flashlight_.setLit(lit);
      break;
  }
}

void NodeOutput::service(uint32_t now) {
  switch (nodeKind_) {
    case decaflash::NodeKind::RgbStrip:
      rgbStrip_.service(now);
      break;

    case decaflash::NodeKind::Flashlight:
    default:
      if (visualState_.flashOverride == decaflash::FlashOverride::Full) {
        flashlight_.setLit(true);
      } else if (visualState_.flashOverride == decaflash::FlashOverride::Off) {
        flashlight_.setLit(false);
      } else {
        flashlight_.service(now);
      }
      break;
  }
}

const char* NodeOutput::rendererName() const {
  return rendererNameFor(nodeKind_);
}

bool NodeOutput::surfaceModulationState(uint32_t now, SurfaceModulationState& state) const {
  if (nodeKind_ != decaflash::NodeKind::RgbStrip) {
    state = {};
    return false;
  }

  state = rgbStrip_.surfaceModulationState(now);
  return state.active;
}
