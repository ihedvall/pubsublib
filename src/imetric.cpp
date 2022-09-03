/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include <util/stringutil.h>
#include "pubsub/imetric.h"
#include "sparkplug_b.pb.h"
#include "payloadhelper.h"
using namespace org::eclipse::tahu::protobuf;

namespace pub_sub {

template<>
void IMetric::Value(std::string value) {
  std::lock_guard lock(value_mutex_);
  value_ = std::move(value);
}

template<>
void IMetric::Value(const char* value) {
  std::lock_guard lock(value_mutex_);
  value_ = value != nullptr ? value : "";
}

template<>
void IMetric::Value(bool value) {
  std::lock_guard lock(value_mutex_);
  value_ = value ? "1" : "0";
}

template<>
void IMetric::Value(float value) {
  std::lock_guard lock(value_mutex_);
  value_ = util::string::FloatToString(value);
}

template<>
void IMetric::Value(double value) {
  std::lock_guard lock(value_mutex_);
  value_ = util::string::DoubleToString(value);
}

template<>
std::string IMetric::Value() const {
  std::lock_guard lock(value_mutex_);
  return value_;
}

template<>
int8_t IMetric::Value() const {
  int8_t temp = 0;
  std::lock_guard lock(value_mutex_);
  try {
    temp = static_cast< int8_t>(std::stoi(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
uint8_t IMetric::Value() const {
  uint8_t temp = 0;
  std::lock_guard lock(value_mutex_);
  try {
    temp = static_cast< uint8_t>(std::stoul(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
bool IMetric::Value() const {
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

void IMetric::GetBody(std::vector<uint8_t> &dest) const {
  Payload_Metric metric;
  PayloadHelper::MetricToProtobuf(*this, metric);
  const size_t body_size = metric.ByteSizeLong();
  dest.resize(body_size);
  metric.SerializeToArray(dest.data(),static_cast<int>(body_size));
}

std::string IMetric::DebugString() const {
  Payload_Metric metric;
  PayloadHelper::MetricToProtobuf(*this, metric);
  return metric.DebugString();
}

void IMetric::AddProperty(const Property& property) {
  auto itr = std::ranges::find_if(property_list_, [&] (const auto& prop) {
    return util::string::IEquals(property.key, prop.key);
  });
  if (itr != property_list_.end()) {
    property_list_.erase(itr);
  }
  property_list_.emplace_back(property);
}

void IMetric::DeleteProperty(const std::string &key) {
  auto itr = std::ranges::find_if(property_list_, [&] (const auto& prop) {
    return util::string::IEquals(key, prop.key);
  });
  if (itr != property_list_.end()) {
    property_list_.erase(itr);
  }
}

} // end namespace