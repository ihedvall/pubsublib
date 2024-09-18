/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <cstdint>
#include <string>
#include <atomic>
#include <vector>
#include <map>
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
enum class MetricType : uint32_t {
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
 * @brief The struct represents a key-value pair with a specific value type.
 *
 * The Property struct contains the following members:
 * - key: The key/name of the property (string).
 * - type: The value type of the property (ValueType enum).
 * - is_null: Indicates whether the property value is null or not (bool).
 * - value: The actual value of the property, represented as a string.
 */
struct MetricProperty {
  std::string key;
  MetricType  type = MetricType::String;
  bool        is_null = false;
  std::string value;
  std::vector<std::map<std::string, MetricProperty>> prop_list;

  template <typename T>
  [[nodiscard]] T Value() const {
    T temp = {};
    try {
      std::istringstream str(value);
      str >> temp;
    } catch (const std::exception& ) {
    }
    return temp;
  }

  template<>
  [[nodiscard]] std::string Value() const;

  template<>
  [[nodiscard]] int8_t Value() const;

  template<>
  [[nodiscard]] uint8_t Value() const;

  template<>
  [[nodiscard]] bool Value() const;

};
using MetricPropertyList = std::map<std::string, MetricProperty>;

/** \brief Payload bytes specific metadata
 *
 */
struct MetricMetaData {
  bool is_multi_part = false; ///< True if this is a multi part message
  std::string content_type;   ///< Content media type string
  uint64_t size = 0;          ///< Number of bytes
  uint64_t seq = 0;           ///< Multi-part sequence number
  std::string file_name;      ///< File name
  std::string file_type;      ///< File type as xml or json
  std::string md5;            ///< MD5 checksum as string
  std::string description;    ///< Description

};


/**
 * @class IMetric
 * @brief The IValue class represents a generic value with various properties.
 *
 * The IValue class provides a flexible interface to a value in a public/subscribe communication
 * interface. The interface are used when configure the system and when reading and updating the value.
 *
 * Note that in the basic MQTT protocol, no properties exist but in SparkPlug B properties are optional.
 * This interface have interfaces to the most common properties as unit and description.
 */

class IMetric {
 public:
  IMetric() = default;
  explicit IMetric(std::string name);
  explicit IMetric(const std::string_view& name);

  void Name(std::string name);
  [[nodiscard]] std::string Name() const;

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

  void Unit(const std::string& unit);
  [[nodiscard]] std::string Unit() const;

  void Type(MetricType type) {
    datatype_ = static_cast<uint32_t >(type);
  }

  [[nodiscard]] MetricType Type() const {
    return static_cast<MetricType>(datatype_.load());
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

  void IsValid(bool valid) const {
    valid_ = valid;
  }
  [[nodiscard]] bool IsValid() const {
    return valid_;
  }

  void IsReadOnly(bool read_only) {
    read_only_ = read_only;
  }
  [[nodiscard]] bool IsReadOnly() const {
    return read_only_;
  }

  [[nodiscard]] MetricMetaData* CreateMetaData();
  [[nodiscard]] MetricMetaData* GetMetaData();
  [[nodiscard]] const MetricMetaData* GetMetaData() const;

  void AddProperty(const MetricProperty& property);
  [[nodiscard]] MetricProperty* CreateProperty(const std::string& key);
  [[nodiscard]] MetricProperty* GetProperty(const std::string& key);
  [[nodiscard]] const MetricProperty* GetProperty(const std::string& key) const;

  const MetricPropertyList& Properties() const {
    return property_list_;
  }
  void DeleteProperty(const std::string& key);

  template <typename T>
  void Value(T value);

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
  void SetPublish(std::function<void(IMetric&)> on_publish) {
    on_publish_ = std::move(on_publish);
  }

  void SetUpdated() { updated_ = true; }
  [[nodiscard]] bool IsUpdated() {
    bool upd = updated_;
    updated_ = false;
    return upd;
  }

 private:
  std::string name_;
  std::atomic<uint64_t> alias_ = 0;
  std::atomic<uint64_t> timestamp_ = 0;
  std::atomic<uint32_t> datatype_ = 0;
  std::atomic<bool> is_historical_ = false;
  std::atomic<bool> is_transient_ = false;
  std::atomic<bool> is_null_ = false;

  mutable std::atomic<bool> valid_ = false; ///< Indicate if the metric is GOOD or STALE
  std::atomic<bool> read_only_ = false; ///< Indicate if the metric can be changes remotely

  MetricPropertyList property_list_;

  mutable std::recursive_mutex metric_mutex_;
  std::string value_;
  std::function<void()> on_update_;
  std::function<void(IMetric& )> on_publish_;
  std::unique_ptr<MetricMetaData> meta_data_;

  std::atomic<bool> updated_ = false;
};

template<typename T>
void IMetric::Value(T value) {
  try {
    std::scoped_lock lock(metric_mutex_);
    value_ = std::to_string(value);
    IsValid(true);
  } catch (const std::exception& ) {
    IsValid(false);
  }
}

template<>
void IMetric::Value(std::string value);

template<>
void IMetric::Value(std::string_view& value);

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

  try {
    std::lock_guard lock(metric_mutex_);
    std::istringstream str(value_);
    str >> temp;
  } catch (const std::exception& ) {
    IsValid(false);
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