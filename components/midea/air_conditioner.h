#pragma once

#if (defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF)

// MideaUART
#include <Appliance/AirConditioner/AirConditioner.h>

#include "appliance_base.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::midea::ac {

using sensor::Sensor;
using climate::ClimateCall;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateMode;
using climate::ClimateSwingMode;
using climate::ClimateFanMode;
using climate::ClimateModeMask;
using climate::ClimateSwingModeMask;
using climate::ClimatePresetMask;

class AirConditioner final : public ApplianceBase<dudanov::midea::ac::AirConditioner>, public climate::Climate {
 public:
  void dump_config() override;
  void setup() override {
    ApplianceBase<dudanov::midea::ac::AirConditioner>::setup();
    if (this->vane_sensor_) {
      this->base_.set_vane_callback([this](float value) { this->vane_sensor_->publish_state(value); });
      this->set_interval("vane_position", this->vane_interval_, [this]() { this->base_.poll_vane(); });
    }
    if (this->diagnostics_enabled_) {
      this->base_.set_diagnostic_callback([this](uint8_t index, float value) {
        if (index < this->diagnostic_sensors_.size() && this->diagnostic_sensors_[index])
          this->diagnostic_sensors_[index]->publish_state(value);
      });
      this->set_interval("extended_diagnostics", 10000, [this]() { this->base_.poll_diagnostics(); });
    }
    if (this->indoor_fan_interval_ != 0 && (this->diagnostic_sensors_[9] || this->diagnostic_sensors_[10])) {
      this->base_.set_indoor_fan_polling(true);
      this->set_interval("indoor_fan", this->indoor_fan_interval_, [this]() { this->base_.poll_indoor_fan(); });
    }
    if (this->compressor_power_sensor_ != nullptr) {
      this->base_.set_power_callback([this](float watts) { this->compressor_power_sensor_->publish_state(watts); });
      this->set_interval("compressor_power", this->compressor_power_interval_, [this]() {
        this->base_.query_compressor_power();
      });
    }
  }
  void set_vane_position_sensor(Sensor *sensor) { vane_sensor_=sensor; }
  void set_vane_position_interval(uint32_t interval) { vane_interval_=interval; }
  void set_indoor_fan_interval(uint32_t interval) { this->indoor_fan_interval_=interval; }
  void set_energy_probe(bool enabled) { this->base_.set_energy_probe(enabled); if (enabled) this->diagnostics_enabled_=true; }
  void set_diagnostic_sensor(uint8_t index, Sensor *sensor) {
    if (index < this->diagnostic_sensors_.size()) { this->diagnostic_sensors_[index] = sensor; this->diagnostics_enabled_ = true; }
  }
  void set_compressor_power_sensor(Sensor *sensor) { this->compressor_power_sensor_ = sensor; }
  void set_compressor_power_interval(uint32_t interval) { this->compressor_power_interval_ = interval; }
  void set_outdoor_temperature_sensor(Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(Sensor *sensor) { this->power_sensor_ = sensor; }
  void on_status_change() override;

  /* ############### */
  /* ### ACTIONS ### */
  /* ############### */

  void do_follow_me(float temperature, bool use_fahrenheit, bool beeper = false);
  void do_display_toggle();
  void do_swing_step();
  void do_beeper_on() { this->set_beeper_feedback(true); }
  void do_beeper_off() { this->set_beeper_feedback(false); }
  void do_power_on() { this->base_.setPowerState(true); }
  void do_power_off() { this->base_.setPowerState(false); }
  void do_power_toggle() { this->base_.setPowerState(this->mode == ClimateMode::CLIMATE_MODE_OFF); }
  void set_supported_modes(ClimateModeMask modes) { this->supported_modes_ = modes; }
  void set_supported_swing_modes(ClimateSwingModeMask modes) { this->supported_swing_modes_ = modes; }
  void set_supported_presets(ClimatePresetMask presets) { this->supported_presets_ = presets; }
  void set_custom_presets(std::initializer_list<const char *> presets) { this->set_supported_custom_presets(presets); }
  void set_custom_fan_modes(std::initializer_list<const char *> modes) { this->set_supported_custom_fan_modes(modes); }

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ClimateModeMask supported_modes_{};
  ClimateSwingModeMask supported_swing_modes_{};
  ClimatePresetMask supported_presets_{};
  bool frost_protection_set_{false};
  Sensor *outdoor_sensor_{nullptr};
  Sensor *humidity_sensor_{nullptr};
  Sensor *power_sensor_{nullptr};
  std::array<Sensor *,16> diagnostic_sensors_{};
  Sensor *vane_sensor_{nullptr};
  uint32_t vane_interval_{1000};
  bool diagnostics_enabled_{false};
  uint32_t indoor_fan_interval_{0};
  Sensor *compressor_power_sensor_{nullptr};
  uint32_t compressor_power_interval_{30000};
};

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
