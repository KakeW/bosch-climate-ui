#pragma once

#if (defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF)

#include "esphome/core/automation.h"
#include "air_conditioner.h"

namespace esphome::midea::ac {

template<typename... Ts> class MideaActionBase : public Action<Ts...> {
 public:
  void set_parent(AirConditioner *parent) { this->parent_ = parent; }

 protected:
  AirConditioner *parent_;
};

template<typename... Ts> class FollowMeAction : public MideaActionBase<Ts...> {
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(bool, use_fahrenheit)
  TEMPLATABLE_VALUE(bool, beeper)

  void play(const Ts &...x) override {
    this->parent_->do_follow_me(this->temperature_.value(x...), this->use_fahrenheit_.value(x...),
                                this->beeper_.value(x...));
  }
};

template<typename... Ts> class QueryCompressorPowerAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_query_compressor_power(); }
};

template<typename... Ts> class DisplayToggleUARTOnceAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_display_toggle_uart_once(); }
};

template<typename... Ts> class SetExtendedAction : public MideaActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, tag)
  TEMPLATABLE_VALUE(uint8_t, value)
  void play(const Ts &...x) override { this->parent_->do_set_extended(this->tag_.value(x...), this->value_.value(x...)); }
};

template<typename... Ts> class SetVaneAction : public MideaActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, position)
  void play(const Ts &...x) override { this->parent_->do_set_vane(this->position_.value(x...)); }
};

template<typename... Ts> class QueryVaneAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_query_vane(); }
};

template<typename... Ts> class SwingStepAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_swing_step(); }
};

template<typename... Ts> class DisplayToggleAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_display_toggle(); }
};

template<typename... Ts> class BeeperOnAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_beeper_on(); }
};

template<typename... Ts> class BeeperOffAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_beeper_off(); }
};

template<typename... Ts> class PowerOnAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_power_on(); }
};

template<typename... Ts> class PowerOffAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_power_off(); }
};

template<typename... Ts> class PowerToggleAction : public MideaActionBase<Ts...> {
 public:
  void play(const Ts &...x) override { this->parent_->do_power_toggle(); }
};

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
