/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <atomic>
#include <utility>
#include <vector>
#include <map>
#include <algorithm>
#include <mutex>
#include <functional>
#include <util/timestamp.h>
#include <util/stringutil.h>
namespace pub_sub {

/**
 * @brief The ValueType enum class defines the various types of values that can be used in a system.
 *
 * The ValueType enum class contains a list of all the supported value types in the system.
 * Each value type is represented by a unique identifier.
 */
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

/**
 * @brief The Property struct represents a key-value pair with a specific value type.
 *
 * The Property struct contains the following members:
 * - key: The key/name of the property (string).
 * - type: The value type of the property (ValueType enum).
 * - is_null: Indicates whether the property value is null or not (bool).
 * - value: The actual value of the property, represented as a string.
 */
struct Property {
  std::string key;
  ValueType   type = ValueType::String;
  bool        is_null = false;
  std::string value;
};

/**
 * @class IValue
 * @brief The IValue class represents a generic value with various properties.
 *
 * The IValue class provides a flexible interface to a value in a public/subscribe communication
 * interface. The interface are used when configure the system and when reading and updating the value.
 *
 * Note that in the basic MQTT protocol, no properties exist but in SparkPlug B properties are optional.
 * This interface have interfaces to the most common properties as unit and description.
 */

class IValue {
 public:
  IValue() = default;
  explicit IValue(std::string name);
  explicit IValue(const std::string_view& name);

  using PropertyList = std::map<std::string, Property>;

  void Name(std::string name) {
    name_ = std::move(name);
  }
  [[nodiscard]] const std::string& Name() const {
    return name_;
  }

  void Unit(const std::string& unit);
  [[nodiscard]] std::string Unit() const;


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
  void Value(const T value);

  template<typename T>
  [[nodiscard]] T Value() const;

  void GetBody(std::vector<uint8_t>& dest) const;
  std::string GetMqttString() const;
  [[nodiscard]] std::string DebugString() const;

  void OnUpdate();
  void SetOnUpdate(std::function<void()> on_update) {
    on_update_ = std::move(on_update);
  }

  void Publish();
  void SetPublish(std::function<void(IValue&)> on_publish) {
    on_publish_ = std::move(on_publish);
  }


 private:
  std::string name_;
  uint64_t alias_ = 0;
  std::atomic<uint64_t> timestamp_ = 0;
  uint32_t datatype_ = 0;
  std::atomic<bool> is_historical_ = false;
  std::atomic<bool> is_transient_ = false;
  std::atomic<bool> is_null_ = true;
  PropertyList property_list_;
  mutable std::recursive_mutex value_mutex_;
  std::string value_;
  std::function<void()> on_update_;
  std::function<void( IValue& )> on_publish_;


};

template<typename T>
void IValue::Value(T value) {
  try {
    is_null_ = false;
    std::lock_guard lock(value_mutex_);
    value_ = std::to_string(value);
  } catch (const std::exception& ) {
    is_null_ = true;
  }
}

template<>
void IValue::Value(std::string value);

template<>
void IValue::Value(std::string_view& value);

template<>
void IValue::Value(const char* value);

template<>
void IValue::Value(bool value);

template<>
void IValue::Value(float value);

template<>
void IValue::Value(double value);

template<typename T>
T IValue::Value() const {
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
std::string IValue::Value() const;

template<>
int8_t IValue::Value() const;

template<>
uint8_t IValue::Value() const;

template<>
bool IValue::Value() const;

} // end namespace