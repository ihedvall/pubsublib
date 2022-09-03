/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <atomic>
#include <vector>
#include <algorithm>
#include <mutex>
#include <util/timestamp.h>
#include <util/stringutil.h>
namespace pub_sub {
enum class ValueType : uint32_t {
  Unknown         = 0,
  // Basic Types
  Int8            = 1,
  Int16           = 2,
  Int32           = 3,
  Int64           = 4,
  UInt8           = 5,
  UInt16          = 6,
  UInt32          = 7,
  UInt64          = 8,
  Float           = 9,
  Double          = 10,
  Boolean         = 11,
  String          = 12,
  DateTime        = 13,
  Text            = 14,
  // Additional Metric Types
  UUID            = 15,
  DataSet         = 16,
  Bytes           = 17,
  File            = 18,
  Template        = 19,

  // Additional PropertyValue Types
  PropertySet     = 20,
  PropertySetList = 21,

  // Array Types
  Int8Array = 22,
  Int16Array = 23,
  Int32Array = 24,
  Int64Array = 25,
  UInt8Array = 26,
  UInt16Array = 27,
  UInt32Array = 28,
  UInt64Array = 29,
  FloatArray = 30,
  DoubleArray = 31,
  BooleanArray = 32,
  StringArray = 33,
  DateTimeArray = 34,
};

struct Property {
  std::string key;
  ValueType   type = ValueType::String;
  bool        is_null = false;
  std::string value;
};

class IMetric {
 public:
  using PropertyList = std::vector<Property>;

  void Name(const std::string& name) {
    name_ = name;
  }
  [[nodiscard]] const std::string& Name() const {
    return name_;
  }

  void Alias(uint64_t alias) {
    alias_ = alias;
  }
  [[nodiscard]] uint64_t Alias() const {
    return alias_;
  }

  void Timestamp(uint64_t ms_since_1970) {
    timestamp_ = ms_since_1970;
  }
  [[nodiscard]] uint64_t Timestamp() const {
    return timestamp_;
  }

  void Type(ValueType type) {
    datatype_ = static_cast<uint32_t >(type);
  }
  [[nodiscard]] ValueType Type() const {
    return static_cast<ValueType>(datatype_);
  }

  void IsHistorical(bool historical_value) {
    is_historical_ = historical_value;
  }
  [[nodiscard]] bool IsHistorical() const {
    return is_historical_;
  }

  void IsTransient(bool transient_value) {
    is_transient_ = transient_value;
  }
  [[nodiscard]] bool IsTransient() const {
    return is_transient_;
  }

  void IsNull(bool null_value) {
    is_null_ = null_value;
  }
  [[nodiscard]] bool IsNull() const {
    return is_null_;
  }

  void AddProperty(const Property& property);
  const PropertyList& Properties() const {
    return property_list_;
  }
  void DeleteProperty(const std::string& key);

  template <typename T>
  void Value(T value);

  template<typename T>
  [[nodiscard]] T Value() const;

  void GetBody(std::vector<uint8_t>& dest) const;
  [[nodiscard]] std::string DebugString() const;
 private:
  std::string name_;
  uint64_t alias_ = 0;
  std::atomic<uint64_t> timestamp_ = 0;
  uint32_t datatype_ = 0;
  std::atomic<bool> is_historical_ = false;
  std::atomic<bool> is_transient_ = false;
  std::atomic<bool> is_null_ = false;
  PropertyList property_list_;
  mutable std::recursive_mutex value_mutex_;
  std::string value_;
};

template<typename T>
void IMetric::Value(T value) {
  std::lock_guard lock(value_mutex_);
  try {
    value_ = std::to_string(value);
  } catch (const std::exception& ) {

  }
}

template<>
void IMetric::Value(std::string value);

template<>
void IMetric::Value(const char* value);

template<>
void IMetric::Value(bool value);

template<>
void IMetric::Value(float value);

template<>
void IMetric::Value(double value);

template<typename T>
T IMetric::Value() const {
  T temp = {};
  std::lock_guard lock(value_mutex_);
  try {
    std::istringstream str(value_);
    str >> temp;
  } catch (const std::exception& ) {
  }
  return temp;
}


template<>
std::string IMetric::Value() const;

template<>
int8_t IMetric::Value() const;

template<>
uint8_t IMetric::Value() const;

template<>
bool IMetric::Value() const;

} // end namespace