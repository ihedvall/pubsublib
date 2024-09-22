/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <mutex>
#include <sstream>
#include <map>
#include <vector>
#include <memory>

#include <util/stringutil.h>
#include "pubsub/metrictype.h"


namespace pub_sub {

class MetricProperty;

using MetricPropertyList = std::map<std::string, MetricProperty,
      util::string::IgnoreCase>;

class MetricProperty {
 public:
  MetricProperty() = default;
  MetricProperty(std::string key, std::string value);

  MetricProperty(const MetricProperty& property);
  MetricProperty& operator = (const MetricProperty& property);

  void Key(const std::string& key) { key_ = key;}
  [[nodiscard]] const std::string& Key() const { return key_;}

  void Type(MetricType type) { type_ = type;}
  [[nodiscard]] MetricType Type() const { return type_;}

  void IsNull(bool is_null);
  [[nodiscard]] bool IsNull() const;

  template <typename T>
  void Value(T value);

  template <typename T>
  [[nodiscard]] T Value() const;

  std::vector<MetricPropertyList>& PropertyArray();
  const std::vector<MetricPropertyList>& PropertyArray() const;
 private:
  std::string key_;
  MetricType  type_ = MetricType::String;
  bool        is_null_ = false;
  std::string value_;

  std::vector< MetricPropertyList > prop_array_;

  mutable std::recursive_mutex property_mutex_;
};


template<typename T>
void MetricProperty::Value(T value) {
  try {
    std::scoped_lock lock(property_mutex_);
    value_ = std::to_string(value);
  } catch (const std::exception&) {}
}

template<>
void MetricProperty::Value(std::string value);

template<>
void MetricProperty::Value(const char* value);

template<>
void MetricProperty::Value(bool value);

template <typename T>
[[nodiscard]] T MetricProperty::Value() const {
  T temp = {};
  try {
    std::istringstream str(value_);
    str >> temp;
  } catch (const std::exception& ) {
  }
  return temp;
}

template<>
[[nodiscard]] std::string  MetricProperty::Value() const;

template<>
[[nodiscard]] int8_t  MetricProperty::Value() const;

template<>
[[nodiscard]] uint8_t  MetricProperty::Value() const;

template<>
[[nodiscard]] bool MetricProperty::Value() const;
} // pub_sub


