#pragma once
#include <Appliance/ApplianceBase.h>
#include <Helpers/Log.h>
#include <cmath>

namespace dudanov::midea {

// midea-local MessageNewProtocolQuery, limited to tag 0x0009.
// Reuse MideaUART's shared message counter, CRC and outer framing.
class VaneQueryData : public FrameData {
 public:
  VaneQueryData() : FrameData({0xB1, 0x01, 0x09, 0x00, m_getID()}) { this->appendCRC(); }
  uint8_t message_id() const { return this->data()[4]; }
};

inline bool valid_vane_position(uint8_t value) {
  return value == 1 || value == 25 || value == 50 || value == 75 || value == 100;
}
// midea-local MessageNewProtocolSet: prompt_tone=false followed by wind_ud_angle.
class VaneSetData : public FrameData {
 public:
  explicit VaneSetData(uint8_t position)
      : FrameData({0xB0, 0x02, 0x1A, 0x00, 0x01, 0x00, 0x09, 0x00, 0x01, position, m_getID()}) {
    this->appendCRC();
  }
  uint8_t message_id() const { return this->data()[10]; }
};

enum class VaneResult { VALUE, ABSENT, MALFORMED, UNRELATED };
struct VaneReply {
  VaneResult result;
  uint8_t value{0};
  uint8_t reserved{0};
};

// B1, echoed ID or observed Bosch zero ID, valid CRC, complete TLVs.
// Zero-ID replies are matched to the active query by type/tag, not unique ID.
inline VaneReply parse_vane_reply(const FrameData &frame, uint8_t message_id, uint8_t body_type = 0xB1) {
  const auto *data = frame.data();
  const size_t size = frame.size();
  if (size == 0 || data[0] != body_type)
    return {VaneResult::UNRELATED};
  if (size < 4 || !frame.hasValidCRC())
    return {VaneResult::MALFORMED};
  if (data[size - 2] != message_id && data[size - 2] != 0)
    return {VaneResult::UNRELATED};
  const size_t end = size - 2;
  size_t pos = 2;
  VaneReply result{VaneResult::ABSENT};
  for (uint8_t i = 0; i < data[1]; ++i) {
    if (end - pos < 4)
      return {VaneResult::MALFORMED};
    const uint16_t tag = data[pos] | (uint16_t(data[pos + 1]) << 8);
    const uint8_t reserved = data[pos + 2];
    const size_t length = data[pos + 3];
    pos += 4;
    if (length > end - pos)
      return {VaneResult::MALFORMED};
    if (tag == 0x0009) {
      if (length != 1 || result.result == VaneResult::VALUE)
        return {VaneResult::MALFORMED};
      result = {VaneResult::VALUE, data[pos], reserved};
    }
    pos += length;
  }
  if (pos != end)
    return {VaneResult::MALFORMED};
  return result;
}

// MessageGroupSevenQuery has no message ID: six payload bytes plus CRC.
class PowerQueryData : public FrameData {
 public:
  PowerQueryData() : FrameData({0x41, 0x21, 0x01, 0x47, 0x00, 0x01}) { this->appendCRC(); }
};

enum class PowerResult { VALUE, MALFORMED, UNRELATED };
struct PowerReply {
  PowerResult result;
  uint16_t watts{0};
};
inline PowerReply parse_power_reply(const FrameData &frame) {
  const auto *data = frame.data();
  const size_t size = frame.size();
  if (size == 0 || data[0] != 0xC1)
    return {PowerResult::UNRELATED};
  if (size < 4)
    return {PowerResult::MALFORMED};
  if (data[3] != 0x47)
    return {PowerResult::UNRELATED};
  // Bytes 10 and 11 must exist independently of the trailing CRC.
  if (size < 13 || !frame.hasValidCRC())
    return {PowerResult::MALFORMED};
  return {PowerResult::VALUE, static_cast<uint16_t>(data[10] | (uint16_t(data[11]) << 8))};
}

template<typename T> class VaneProbeAppliance : public T {
 public:
  void set_vane_callback(std::function<void(float)> cb) { vane_callback_=std::move(cb); }
  bool vane_busy() const { return pending_ || set_pending_; }
  void query_vane() {
    if (this->pending_ || this->set_pending_) {
      LOG_W("midea.vane", "A vane query is already pending");
      return;
    }
    if (this->getAutoconfStatus() == AUTOCONF_PROGRESS) {
      LOG_W("midea.vane", "Wait for Midea autoconfiguration before querying");
      return;
    }
    VaneQueryData query;
    const uint8_t message_id = query.message_id();
    this->pending_ = true;
    LOG_I("midea.vane", "Queueing read-only B1 query: tag=0x0009 id=%u", message_id);
    this->m_queueRequest(
        DEVICE_QUERY, std::move(query),
        [this, message_id](FrameData frame) {
          const auto result = parse_vane_reply(frame, message_id);
          if (result.result == VaneResult::UNRELATED)
            return RESPONSE_WRONG;
          if (result.result == VaneResult::MALFORMED) {
            LOG_W("midea.vane", "Malformed B1 reply or invalid CRC; inspect ApplianceBase RX log");
            return RESPONSE_WRONG;
          }
          if (result.result == VaneResult::ABSENT) {
            LOG_I("midea.vane", "B1 reply received, but tag 0x0009 is absent (support unconfirmed)");
          } else {
            LOG_I("midea.vane", "B1 reply: wind_ud_angle raw=%u reserved=0x%02X request_id=%u reply_id=%u",
                  result.value, result.reserved, message_id, frame.data()[frame.size() - 2]);
          }
          if (vane_callback_) vane_callback_(result.result == VaneResult::VALUE && result.reserved == 0 ? float(result.value) : NAN);
          return RESPONSE_OK;
        },
        [this]() { this->finish_vane_query_(); },
        [this]() {
          if (vane_callback_) vane_callback_(NAN);
          this->finish_vane_query_();
          LOG_W("midea.vane", "No matching valid B1 reply; support remains unconfirmed. Inspect TX/RX log");
        });
  }
  void set_vane(uint8_t position) {
    if (!valid_vane_position(position)) {
      LOG_W("midea.vane", "Rejected invalid vane position=%u", position);
      return;
    }
    if (this->pending_ && !this->set_pending_) {
      deferred_position_=position; // Preserve user control during periodic readback.
      return;
    }
    if (this->set_pending_ || this->pending_) {
      LOG_W("midea.vane", "Wait for the pending vane operation before changing position");
      return;
    }
    if (this->getAutoconfStatus() == AUTOCONF_PROGRESS) {
      LOG_W("midea.vane", "Wait for Midea autoconfiguration before setting vane");
      return;
    }
    VaneSetData request(position);
    const uint8_t message_id = request.message_id();
    this->set_pending_ = true;
    LOG_I("midea.vane", "Queueing B0 SET: wind_ud_angle=%u prompt_tone=false id=%u", position, message_id);
    this->m_queueRequest(
        DEVICE_CONTROL, std::move(request),
        [position, message_id](FrameData frame) {
          const auto result = parse_vane_reply(frame, message_id, 0xB0);
          if (result.result == VaneResult::UNRELATED)
            return RESPONSE_WRONG;
          if (result.result == VaneResult::MALFORMED) {
            LOG_W("midea.vane", "Malformed B0 SET reply; inspect RX log");
            return RESPONSE_WRONG;
          }
          if (result.result == VaneResult::VALUE) {
            LOG_I("midea.vane", "B0 SET reply: requested=%u returned=%u reserved=0x%02X (physical position unverified)",
                  position, result.value, result.reserved);
          } else {
            LOG_W("midea.vane", "B0 SET reply has no vane value; acceptance unconfirmed");
          }
          return RESPONSE_OK;
        },
        [this]() {
          this->set_pending_ = false;
          this->query_vane();
        },
        [this]() {
          this->set_pending_ = false;
          LOG_W("midea.vane", "No valid B0 SET reply; querying vane to inspect state");
          this->query_vane();
        });
  }
  void set_power_callback(std::function<void(float)> callback) { this->power_callback_ = std::move(callback); }
  void query_compressor_power() {
    if (this->power_pending_) {
      LOG_W("midea.power_probe", "A compressor power query is already pending");
      return;
    }
    if (this->getAutoconfStatus() == AUTOCONF_PROGRESS) {
      LOG_W("midea.power_probe", "Wait for Midea autoconfiguration before querying");
      return;
    }
    this->power_pending_ = true;
    LOG_I("midea.power_probe", "Queueing read-only compressor power query: group=0x47");
    this->m_queueRequest(
        DEVICE_QUERY, PowerQueryData(),
        [this](FrameData frame) {
          const auto result = parse_power_reply(frame);
          if (result.result == PowerResult::UNRELATED)
            return RESPONSE_WRONG;
          if (result.result == PowerResult::MALFORMED) {
            LOG_W("midea.power_probe", "Malformed C1/47 reply or invalid CRC; inspect ApplianceBase RX log");
            return RESPONSE_WRONG;
          }
          LOG_I("midea.power_probe", "C1/47 reply: compressor_power=%u W (Bosch interpretation unverified)",
                result.watts);
          if (this->power_callback_) this->power_callback_(result.watts);
          return RESPONSE_OK;
        },
        [this]() { this->power_pending_ = false; },
        [this]() {
          this->power_pending_ = false;
          if (this->power_callback_) this->power_callback_(NAN);
          LOG_W("midea.power_probe", "No valid C1/47 reply; support remains unconfirmed. Inspect TX/RX log");
        });
  }
 private:
  void finish_vane_query_() {
    pending_=false;
    if (deferred_position_ >= 0) { auto value=deferred_position_; deferred_position_=-1; set_vane(value); }
  }
  int deferred_position_{-1};
  std::function<void(float)> vane_callback_;
  std::function<void(float)> power_callback_;
  bool set_pending_{false};
  bool power_pending_{false};
  bool pending_{false};
};
}  // namespace dudanov::midea
