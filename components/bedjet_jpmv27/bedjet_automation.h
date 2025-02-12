#pragma once
#ifdef USE_ESP32

#include "esphome/core/automation.h"

namespace esphome {
namespace bedjet {

class UpdateTrigger : public Trigger<> {
 public:
  void process() { this->trigger(); }
};

}  // namespace bedjet
}  // namespace esphome

#endif // USE_ESP32

