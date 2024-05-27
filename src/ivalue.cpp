/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include "pubsub/ivalue.h"

#include <util/stringutil.h>

#include <utility>

#include "sparkplug_b_c_sharp.pb.h"
#include "payloadhelper.h"
using namespace org::eclipse::tahu::protobuf;

namespace pub_sub {

IValue::IValue(std::string  name)
  : name_(std::move(name)) {

}
IValue::IValue(const std::string_view& name)
    : name_(name.data()) {

}

/** @brief In MQTT the value are sent as string value. Sometimes the value is appended with
 * a unit string.
 *
 * The MQTT payload normally uses string values to send values. Sometimes a unit string is appended
 * to the string. So if the value is a decimal value, search for an optional unit string.
 * @param value String value with optional unit
 */
template<>
void IValue::Value(std::string value) {
  // Note: Special handling for MQTT if the value is appended with unit.
  const auto type = static_cast<uint32_t>(datatype_);
  if (type > static_cast<uint32_t>(ValueType::Unknown) && type <= static_cast<uint32_t>(ValueType::Double)) {
    // Check for an optional unit string
    const auto space = value.find_first_of(' ');
    if (space != std::string::npos) {
      auto exist = property_list_.find("unit");
      if (exist == property_list_.cend()) {
        Unit(value.substr(space + 1));
      }
      value = value.substr(0, space);
    }
  }

  is_null_ = false;
  std::lock_guard lock(value_mutex_);
  value_ = std::move(value);
}

template<>
void IValue::Value(std::string_view value) {
  is_null_ = false;
  std::lock_guard lock(value_mutex_);
  value_ = value;
}

template<>
void IValue::Value(const char* value) {
  is_null_ = false;
  std::lock_guard lock(value_mutex_);
  value_ = value != nullptr ? value : "";
}

template<>
void IValue::Value(bool value) {
  is_null_ = false;
  std::lock_guard lock(value_mutex_);
  value_ = value ? "1" : "0";
}

template<>
void IValue::Value(float value) {
  is_null_ = false;
  std::lock_guard lock(value_mutex_);
  value_ = util::string::FloatToString(value);
}

template<>
void IValue::Value(double value) {
  is_null_ = false;
  std::lock_guard lock(value_mutex_);
  value_ = util::string::DoubleToString(value);
}

template<>
std::string IValue::Value() const {
  std::lock_guard lock(value_mutex_);
  return value_;
}

template<>
int8_t IValue::Value() const {
  int8_t temp = 0;
  std::lock_guard lock(value_mutex_);
  try {
    temp = static_cast< int8_t>(std::stoi(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
uint8_t IValue::Value() const {
  uint8_t temp = 0;
  std::lock_guard lock(value_mutex_);
  try {
    temp = static_cast< uint8_t>(std::stoul(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
bool IValue::Value() const {
  std::lock_guard lock(value_mutex_);
  if (value_.empty()) {
    return false;
  }
  switch (value_[0]) {
    case 'Y':
    case 'y':
    case 'T':
    case 't':
    case '1':
      return true;

    default:
      break;
  }
  return false;
}

void IValue::GetBody(std::vector<uint8_t> &dest) const {
  Payload_Metric metric;
  PayloadHelper::MetricToProtobuf(*this, metric);
  const size_t body_size = metric.ByteSizeLong();
  dest.resize(body_size);
  metric.SerializeToArray(dest.data(),static_cast<int>(body_size));
}

std::string IValue::DebugString() const {
  Payload_Metric metric;
  PayloadHelper::MetricToProtobuf(*this, metric);
  return metric.DebugString();
}

void IValue::AddProperty(const Property& property) {
  auto exist = property_list_.find(property.key);
  if (exist != property_list_.end()) {
    exist->second = property;
  }
  property_list_.emplace(property.key,property);
}

void IValue::DeleteProperty(const std::string &key) {
  auto itr = property_list_.find(key);
  if (itr != property_list_.end()) {
    property_list_.erase(itr);
  }
}

void IValue::Unit(const std::string &name) {
  Property prop;
  prop.key = "unit";
  prop.value = name;
  AddProperty(prop);
}

std::string IValue::Unit() const {
  const auto exist = property_list_.find("unit");
  return exist != property_list_.cend() ? exist->second.value : std::string() ;
}

std::string IValue::GetMqttString() const {
  const auto unit = Unit();
  std::ostringstream text;
  if (!is_null_) {
    std::lock_guard lock(value_mutex_);
    text << value_;
    if (!unit.empty()) {
      text << " " << unit;
    }
  }

  return text.str();
}

void IValue::OnUpdate() {
  if (on_update_) {
    on_update_();
  }
}

void IValue::Publish() {
  if (on_publish_) {
    on_publish_(*this);
  }
}

} // end namespace