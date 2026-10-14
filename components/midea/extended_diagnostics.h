#pragma once
#include "vane_probe.h"
#include <Appliance/AirConditioner/StatusData.h>
#include <array>
#include <cstdio>
#include <string>

namespace dudanov::midea {
// Source: pinned midea-local MessageGroupDataQuery/NewProtocolSet and XC1MessageBody.
class DiagnosticGroupQuery : public FrameData {
 public:
  explicit DiagnosticGroupQuery(uint8_t group) : FrameData({0x41,0x21,0x01,static_cast<uint8_t>(0x40|group),0,1}) { appendCRC(); }
};
class DiagnosticTagQuery : public FrameData {
 public:
  explicit DiagnosticTagQuery(uint16_t tag) : FrameData({0xB1,1,static_cast<uint8_t>(tag),static_cast<uint8_t>(tag>>8),m_getID()}) { appendCRC(); }
  uint8_t message_id() const { return data()[4]; }
};
class DiagnosticTagSet : public FrameData {
 public:
  DiagnosticTagSet(uint16_t tag,uint8_t value) : FrameData({0xB0,2,0x1A,0,1,0,static_cast<uint8_t>(tag),static_cast<uint8_t>(tag>>8),1,value,m_getID()}) { appendCRC(); }
  uint8_t message_id() const { return data()[10]; }
};
struct DiagnosticReply {
  enum Result { VALUE, ABSENT, MALFORMED, UNRELATED } result;
  uint8_t value{0};
  uint8_t reserved{0};
};
inline DiagnosticReply parse_diagnostic_tag(const FrameData &frame,uint16_t wanted,uint8_t id,uint8_t type=0xB1) {
  auto *p=frame.data(); size_t n=frame.size();
  if (!n || p[0]!=type) return {DiagnosticReply::UNRELATED};
  if (n<4 || !frame.hasValidCRC()) return {DiagnosticReply::MALFORMED};
  if (p[n-2]!=0 && p[n-2]!=id) return {DiagnosticReply::UNRELATED};
  size_t pos=2,end=n-2;
  DiagnosticReply result{DiagnosticReply::ABSENT};
  bool seen=false;
  for (unsigned i=0;i<p[1];++i) {
    if (end-pos<4) return {DiagnosticReply::MALFORMED};
    uint16_t tag=p[pos]|(uint16_t(p[pos+1])<<8);
    uint8_t reserved=p[pos+2]; size_t len=p[pos+3];pos+=4;
    if (len>end-pos) return {DiagnosticReply::MALFORMED};
    if (tag==wanted) {
      if (len>1 || seen) return {DiagnosticReply::MALFORMED};
      seen=true;
      result=len==0 ? DiagnosticReply{DiagnosticReply::ABSENT,0,reserved} : DiagnosticReply{DiagnosticReply::VALUE,p[pos],reserved};
    }
    pos+=len;
  }
  if (pos!=end) return {DiagnosticReply::MALFORMED};
  return result;
}
struct DiagnosticGroupReply {
  DiagnosticReply::Result result;
  std::array<float,12> values;
};
inline DiagnosticGroupReply parse_diagnostic_group(const FrameData &frame,uint8_t group) {
  DiagnosticGroupReply out{DiagnosticReply::UNRELATED,{}}; out.values.fill(NAN);
  const auto *p=frame.data();size_t n=frame.size();
  if (!n || p[0]!=0xC1) return out;
  if (n<4) {out.result=DiagnosticReply::MALFORMED;return out;}
  if (p[3]!=(0x40|group) && !(group==3 && p[3]==3)) return out;
  const size_t minimum=group==1?16:group==2?10:group==3?12:256; // Include CRC.
  if (n<minimum || !frame.hasValidCRC()) {out.result=DiagnosticReply::MALFORMED;return out;}
  out.result=DiagnosticReply::VALUE;
  if (group==1) {
    out.values[0]=p[4];out.values[1]=p[5];
    out.values[2]=p[7];out.values[3]=p[8]; // Raw current/voltage: no assumed Bosch scaling.
    out.values[4]=(int(p[10])-30)/2.0f;out.values[5]=(int(p[11])-30)/2.0f;
    out.values[6]=(int(p[12])-50)/2.0f;out.values[7]=(int(p[13])-50)/2.0f;
    out.values[8]=p[14];
  } else if (group==2) {out.values[9]=p[5]*8;out.values[10]=p[4]*8;}
  else {out.values[11]=p[10]*8;}
  return out;
}
inline bool diagnostic_set_allowed(uint16_t tag,uint8_t value) {
  return (tag==0xCD && (value==0 || value==3)) ||
         (tag==0x17 && (value==0 || value==100)) ||
         (tag==0x39 && (value==0 || value==1));
}
template<typename T> class ExtendedDiagnosticAppliance : public VaneProbeAppliance<T> {
 public:
  void set_diagnostic_callback(std::function<void(uint8_t,float)> cb) { callback_=std::move(cb); }
  void set_energy_probe(bool enabled) { energy_probe_=enabled; }
  void set_indoor_fan_polling(bool enabled) { indoor_fan_polling_=enabled; }
  void poll_vane() {
    // Low priority: skip rather than accumulate requests behind other diagnostics.
    if (busy_ || diagnostics_due_ || this->vane_busy() || this->getAutoconfStatus()==AUTOCONF_PROGRESS) return;
    this->query_vane();
  }
  void poll_indoor_fan() {
    if (busy_ || this->getAutoconfStatus()==AUTOCONF_PROGRESS) return;
    query_group_(2);
  }
  // Round-robin read-only queries; at most one extended request pending.
  void poll_diagnostics() {
    if (this->getAutoconfStatus()==AUTOCONF_PROGRESS) return;
    if (busy_) { diagnostics_due_=true; return; }
    diagnostics_due_=false;
    uint8_t source=next_;next_=(next_+1)%(energy_probe_?8:7);
    if (source==1 && indoor_fan_polling_) return; // Keep other diagnostic slots unchanged.
    if (source==7) query_energy_();
    else if (source<3) query_group_(source+1);
    else query_tag_(source==3?0x15:source==4?0xCD:source==5?0x17:0x39);
  }
  void display_toggle_uart_once() {
    if (this->getAutoconfStatus()==AUTOCONF_PROGRESS) {LOG_W("midea.extra","Wait for autoconfiguration");return;}
    LOG_I("midea.extra","Queueing legacy UART display toggle ONCE, no automatic retries");
    // Toggle is not idempotent: a retry could undo a successful first toggle.
    this->m_queueNotify(DEVICE_QUERY,ac::DisplayToggleData());
  }
  void set_extended(uint16_t tag,uint8_t value) {
    if (!diagnostic_set_allowed(tag,value)) {LOG_W("midea.extra","Rejected unsupported SET tag/value");return;}
    if (this->getAutoconfStatus()==AUTOCONF_PROGRESS) {LOG_W("midea.extra","Wait for autoconfiguration before control");return;}
    if (busy_) {
      deferred_=[this,tag,value]() {this->set_extended(tag,value);};
      LOG_I("midea.extra","Control queued after current query (latest request wins)");return;
    }
    busy_=true;DiagnosticTagSet request(tag,value);uint8_t id=request.message_id();
    LOG_I("midea.extra","SET tag=0x%04X value=%u prompt_tone=false",tag,value);
    this->m_queueRequest(DEVICE_CONTROL,std::move(request),
      [tag,id](FrameData data) {
        auto reply=parse_diagnostic_tag(data,tag,id,0xB0);
        if (reply.result==DiagnosticReply::UNRELATED || reply.result==DiagnosticReply::MALFORMED) return RESPONSE_WRONG;
        if (reply.reserved!=0) LOG_W("midea.extra","SET reply tag=0x%04X status_byte=0x%02X (meaning unverified); change NOT confirmed",tag,reply.reserved);
        else if (reply.result==DiagnosticReply::VALUE) LOG_I("midea.extra","SET reply tag=0x%04X value=%u reserved=%u; readback follows",tag,reply.value,reply.reserved);
        else LOG_W("midea.extra","SET reply without requested tag; acceptance unconfirmed");
        return RESPONSE_OK;
      },
      [this,tag]() {busy_=false;query_tag_(tag);},
      [this,tag]() {busy_=false;LOG_W("midea.extra","SET timeout tag=0x%04X; reading state",tag);query_tag_(tag);});
  }
 private:
  void finish_() {
    busy_=false;
    if (deferred_) {auto next=std::move(deferred_);deferred_=nullptr;next();}
    // A timer collision must not discard a whole diagnostic turn indefinitely.
    // User controls/readback retain priority; their finish will drain this flag.
    if (!busy_ && diagnostics_due_) poll_diagnostics();
  }
  void publish_(uint8_t index,float value) {if(callback_)callback_(index,value);}
  void invalidate_group_(uint8_t group) {
    unsigned begin=group==1?0:group==2?9:11,end=group==1?9:group==2?11:12;
    for(unsigned i=begin;i<end;++i)publish_(i,NAN);
  }
  void query_group_(uint8_t group) {
    busy_=true;LOG_D("midea.extra","Query group=%u",group);
    this->m_queueRequest(DEVICE_QUERY,DiagnosticGroupQuery(group),
      [this,group](FrameData data) {
        auto reply=parse_diagnostic_group(data,group);
        if(reply.result!=DiagnosticReply::VALUE)return RESPONSE_WRONG;
        unsigned begin=group==1?0:group==2?9:11,end=group==1?9:group==2?11:12;
        for(unsigned i=begin;i<end;++i)publish_(i,reply.values[i]);
        LOG_I("midea.extra","Group %u received (Bosch field interpretation requires verification)",group);
        return RESPONSE_OK;
      },[this](){finish_();},[this,group](){invalidate_group_(group);LOG_W("midea.extra","No valid response for group %u",group);finish_();});
  }
  void query_energy_() {
    busy_=true;
    LOG_I("midea.extra","Query C1/44 energy probe (raw fields, no assumed units)");
    this->m_queueRequest(DEVICE_QUERY,DiagnosticGroupQuery(4),
      [](FrameData data) {
        const auto *p=data.data(); const size_t n=data.size();
        if (n<4 || p[0]!=0xC1 || p[3]!=0x44 || !data.hasValidCRC()) return RESPONSE_WRONG;
        // Log the full CRC-validated body, including short/unsupported replies.
        std::string raw;
        for (size_t i=0;i<n;++i) { char hex[4]; snprintf(hex,sizeof(hex),"%02X ",p[i]); raw+=hex; }
        LOG_I("midea.extra","C1/44 raw body (%u bytes): %s",unsigned(n),raw.c_str());
        if (n<20) {
          LOG_W("midea.extra","C1/44 response too short for energy fields; support unconfirmed");
          return RESPONSE_OK;
        }
        LOG_I("midea.extra","C1/44 total_energy raw=%02X %02X %02X %02X; operating_energy raw=%02X %02X %02X %02X",p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11]);
        LOG_I("midea.extra","C1/44 current_energy raw=%02X %02X %02X %02X; realtime_power raw=%02X %02X %02X (encoding/scaling unverified)",p[12],p[13],p[14],p[15],p[16],p[17],p[18]);
        return RESPONSE_OK;
      },[this](){finish_();},[this](){LOG_W("midea.extra","No valid C1/44 energy response");finish_();});
  }
  void query_tag_(uint16_t tag) {
    busy_=true;DiagnosticTagQuery request(tag);uint8_t id=request.message_id();
    uint8_t index=tag==0x15?12:tag==0xCD?13:tag==0x17?14:15;
    LOG_D("midea.extra","Query tag=0x%04X",tag);
    this->m_queueRequest(DEVICE_QUERY,std::move(request),
      [this,tag,index,id](FrameData data){
        auto reply=parse_diagnostic_tag(data,tag,id);
        if(reply.result==DiagnosticReply::UNRELATED || reply.result==DiagnosticReply::MALFORMED)return RESPONSE_WRONG;
        if(reply.result==DiagnosticReply::ABSENT || reply.reserved!=0) {
          publish_(index,NAN);LOG_W("midea.extra","Tag 0x%04X returned no usable value (empty/missing or status byte 0x%02X)",tag,reply.reserved);
        } else {
          float value=reply.value;
          if(tag==0x15 && (value<1 || value>100))value=NAN;
          publish_(index,value);LOG_I("midea.extra","Tag 0x%04X raw=%u",tag,reply.value);
        }
        return RESPONSE_OK;
      },[this](){finish_();},[this,tag,index](){publish_(index,NAN);LOG_W("midea.extra","No valid response for tag 0x%04X",tag);finish_();});
  }
  std::function<void()> deferred_;
  bool diagnostics_due_{false};
  bool indoor_fan_polling_{false};
  bool energy_probe_{false};
  bool busy_{false};uint8_t next_{0};
  std::function<void(uint8_t,float)> callback_;
};
}  // namespace dudanov::midea
