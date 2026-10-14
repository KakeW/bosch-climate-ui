#pragma once
#include <cmath>

namespace esphome::midea::ac {
// MideaUART exposes only the converted C0 outdoor reading, not the raw byte.
// 0xFF converts to 102.5 without a decimal nibble, or 102.1..103.5
// with one. 0xFE overlaps this range, so reject the entire marker range.
inline float valid_outdoor_temperature(float value) {
  return value >= 102.0f && value <= 103.5f ? NAN : value;
}
}
