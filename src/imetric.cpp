/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */


#include "pubsub/imetric.h"

#include <util/stringutil.h>

#include <utility>

#include "sparkplug_b.pb.h"
#include "payloadhelper.h"

using namespace org::eclipse::tahu::protobuf;

namespace pub_sub {

template<>
bool MetricProperty::Value() const {
  if (value.empty()) {
    return false;
  }
  switch (value[0]) {
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

template<>
uint8_t MetricProperty::Value() const {
  uint8_t temp = 0;
  try {
    temp = static_cast< uint8_t>(std::stoul(value));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
int8_t MetricProperty::Value() const {
  int8_t temp = 0;
  try {
    temp = static_cast< int8_t>(std::stoi(value));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
std::string MetricProperty::Value() const {
  return value;
}

IMetric::IMetric(std::string  name)
  : name_(std::move(name)) {

}
IMetric::IMetric(const std::string_view& name)
    : name_(name.data()) {

}

void IMetric::Name(std::string name) {
  std::scoped_lock lock(metric_mutex_);
  name_ = std::move(name);
}

std::string IMetric::Name() const {
  std::scoped_lock lock(metric_mutex_);
  return name_;
}

/** @brief In MQTT the value are sent as string value. Sometimes the value is appended with
 * a unit string.
 *
 * The MQTT payload normally uses string values to send values. Sometimes a unit string is appended
 * to the string. So if the value is a decimal value, search for an optional unit string.
 * @param value String value with optional unit
 */
template<>
void IMetric::Value(std::string value) {
  // Note: Special handling for MQTT if the value is appended with unit.
  const auto type = static_cast<uint32_t>(datatype_);
  if (type > static_cast<uint32_t>(MetricType::Unknown) && type <= static_cast<uint32_t>(MetricType::Double)) {
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
  IsValid(true);
  std::scoped_lock lock(metric_mutex_);
  value_ = std::move(value);
}

template<>
void IMetric::Value(std::string_view value) {
  {
    std::scoped_lock lock(metric_mutex_);
    value_ = value;
  }
  IsValid(true);
}

template<>
void IMetric::Value(const char* value) {
  {
    std::scoped_lock lock(metric_mutex_);
    value_ = value != nullptr ? value : "";
  }
  IsValid(true);
}

template<>
void IMetric::Value(bool value) {
  {
    std::scoped_lock lock(metric_mutex_);
    value_ = value ? "1" : "0";
  }
  IsValid(true);
}

template<>
void IMetric::Value(float value) {
  {
    std::scoped_lock lock(metric_mutex_);
    value_ = util::string::FloatToString(value);
    const auto pos = value_.find(',');
    if (pos != std::string::npos) {
      value_.replace(pos, 1, ".");
    }
  }
  IsValid(true);
}

template<>
void IMetric::Value(double value) {
  {
    std::scoped_lock lock(metric_mutex_);
    value_ = util::string::DoubleToString(value);
    const auto pos = value_.find(',');
    if (pos != std::string::npos) {
      value_.replace(pos, 1, ".");
    }
  }
  IsValid(true);
}

template<>
std::string IMetric::Value() const {
  std::scoped_lock lock(metric_mutex_);
  return value_;
}

template<>
int8_t IMetric::Value() const {
  int8_t temp = 0;
  std::scoped_lock lock(metric_mutex_);
  try {
    temp = static_cast< int8_t>(std::stoi(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
uint8_t IMetric::Value() const {
  uint8_t temp = 0;
  std::scoped_lock lock(metric_mutex_);
  try {
    temp = static_cast< uint8_t>(std::stoul(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
bool IMetric::Value() const {
  std::scoped_lock lock(metric_mutex_);
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

void IMetric::GetBody(std::vector<uint8_t> &dest) const {
  Payload_Metric metric;
  IPayload payload;
  PayloadHelper helper(payload);
  helper.WriteAllMetrics(true);
  helper.WriteMetric(*this, metric);

  const size_t body_size = metric.ByteSizeLong();
  dest.resize(body_size);
  metric.SerializeToArray(dest.data(),static_cast<int>(body_size));
}

std::string IMetric::DebugString() const {
  Payload_Metric metric;
  IPayload payload;
  PayloadHelper helper(payload);
  helper.WriteAllMetrics(true);
  {
    std::scoped_lock lock(metric_mutex_);
    helper.WriteMetric(*this, metric);
  }
  return metric.DebugString();
}

void IMetric::AddProperty(const MetricProperty &property) {
  std::scoped_lock lock(metric_mutex_);
  auto exist = property_list_.find(property.key);
  if (exist == property_list_.end()) {
    property_list_.emplace(property.key,property);
  } else {
    exist->second = property;
  }
}
MetricProperty* IMetric::CreateProperty(const std::string& key) {
  std::scoped_lock lock(metric_mutex_);
  if (const auto exist = property_list_.find(key);
      exist == property_list_.cend()) {
    MetricProperty temp;
    temp.key = key;
    property_list_.emplace(key, temp);
  }
  return GetProperty(key);
}

MetricProperty *IMetric::GetProperty(const std::string &key) {
  std::scoped_lock lock(metric_mutex_);
  if (auto exist = property_list_.find(key);
      exist != property_list_.end()) {
    return &exist->second;
  }
  return nullptr;
}

const MetricProperty *IMetric::GetProperty(const std::string &key) const {
  std::scoped_lock lock(metric_mutex_);
  if (const auto exist = property_list_.find(key);
      exist != property_list_.cend()) {
    return &exist->second;
  }
  return nullptr;
}

void IMetric::DeleteProperty(const std::string &key) {
  std::scoped_lock lock(metric_mutex_);
  auto itr = property_list_.find(key);
  if (itr != property_list_.end()) {
    property_list_.erase(itr);
  }
}

void IMetric::Unit(const std::string &name) {
  MetricProperty prop;
  prop.key = "unit";
  prop.value = name;
  std::scoped_lock lock(metric_mutex_);
  AddProperty(prop);
}

std::string IMetric::Unit() const {
  std::scoped_lock lock(metric_mutex_);
  if (const auto exist = property_list_.find("unit");
      exist != property_list_.cend() ) {
    const auto& prop = exist->second;
    try {
      return prop.value;
    } catch (const std::exception&) {
      return {};
    }
  }
  return {};
}

std::string IMetric::GetMqttString() const {
  const auto unit = Unit();
  std::ostringstream text;
  if (!is_null_) {
    text << Value<std::string>();
    if (!unit.empty()) {
      text << " " << unit;
    }
  }
  return text.str();
}

void IMetric::OnUpdate() {
  if (on_update_) {
    on_update_();
  }
}

void IMetric::Publish() {
  if (on_publish_) {
    on_publish_(*this);
  }
}

MetricMetaData *IMetric::CreateMetaData() {
  std::scoped_lock lock(metric_mutex_);
  if (!meta_data_) {
    meta_data_ = std::make_unique<MetricMetaData>();
  }
  return meta_data_.get();
}

MetricMetaData *IMetric::GetMetaData() {
  std::scoped_lock lock(metric_mutex_);
  return meta_data_.get();
}

const MetricMetaData *IMetric::GetMetaData() const {
  std::scoped_lock lock(metric_mutex_);
  return meta_data_.get();
}



} // end namespace