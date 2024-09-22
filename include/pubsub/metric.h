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

#include "pubsub/metrictype.h"
#include "pubsub/metricproperty.h"
#include "pubsub/metricmetadata.h"

namespace pub_sub {

/**
 * @class Metric
 * @brief The Metric class represents a generic value with various properties.
 *
 * The IValue class provides a flexible interface to a value in a public/subscribe communication
 * interface. The interface are used when configure the system and when reading and updating the value.
 *
 * Note that in the basic MQTT protocol, no properties exist but in SparkPlug B properties are optional.
 * This interface have interfaces to the most common properties as unit and description.
 */

class Metric {
 public:
  Metric() = default;
  explicit Metric(std::string name);
  explicit Metric(const std::string_view& name);

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

  void IsReadWrite(bool read_only) {
    read_only_ = read_only;
  }
  [[nodiscard]] bool IsReadOnly() const {
    return read_only_;
  }

  [[nodiscard]] MetricMetadata* CreateMetaData();
  [[nodiscard]] MetricMetadata* GetMetaData();
  [[nodiscard]] const MetricMetadata* GetMetaData() const;

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
  void SetPublish(std::function<void(Metric&)> on_publish) {
    on_publish_ = std::move(on_publish);
  }

  void SetUpdated() { updated_ = true; }
  [[nodiscard]] bool IsUpdated() {
    const bool upd = updated_;
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
  std::function<void(Metric& )> on_publish_;
  std::unique_ptr<MetricMetadata> meta_data_;

  std::atomic<bool> updated_ = false;
};

template<typename T>
void Metric::Value(T value) {
  try {
    {
      std::scoped_lock lock(metric_mutex_);
      value_ = std::to_string(value);
    }
    IsValid(true);
    SetUpdated();
  } catch (const std::exception& ) {
    IsValid(false);
  }
}

template<>
void Metric::Value(std::string value);

template<>
void Metric::Value(std::string_view& value);

template<>
void Metric::Value(const char* value);

template<>
void Metric::Value(bool value);

template<>
void Metric::Value(float value);

template<>
void Metric::Value(double value);

template<typename T>
T Metric::Value() const {
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
std::string Metric::Value() const;

template<>
int8_t Metric::Value() const;

template<>
uint8_t Metric::Value() const;

template<>
bool Metric::Value() const;

} // end namespace