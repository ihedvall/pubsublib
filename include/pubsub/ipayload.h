/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */


#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include <util/timestamp.h>
#include <util/stringutil.h>
#include "pubsub/ivalue.h"

namespace pub_sub {

class IPayload {
 public:
  using BodyList = std::vector<uint8_t>;
  /** \brief Metric/Value list that is sorted on names.
   *
   * List of metrics also known as values. The list is sorted on names
   * and it ignore case. Although case sensitive names are valid, it's
   * a really bad design that causes many issues.
   */
  using MetricList = std::map<std::string, std::unique_ptr<IValue>,
      util::string::IgnoreCase>;

  /** \brief Sets the timestamp for the payload.
   *
   * Sets the timestamp for the payload. Note that bare MQTT doesn't
   * define any timestamp in the payload while Sparkplug B always have
   * one timestamp defines.
   *
   * In MQTT, the timestamp is set when the payload arrive
   * @param ms_since_1970
   */
  void Timestamp(uint64_t ms_since_1970) {
    timestamp_ = ms_since_1970;
    auto* timestamp = GetMetric("timestamp");
    if (timestamp != nullptr) {
      timestamp->Value(ms_since_1970);
    }
  }

  [[nodiscard]] uint64_t Timestamp() const {
    const auto* timestamp = GetMetric("timestamp");
    return timestamp != nullptr ? timestamp->Value<uint64_t>() : timestamp_.load();
  }

  void SequenceNumber(uint64_t seq_no) const {
    sequence_number_ = seq_no;
  }
  [[nodiscard]] uint64_t SequenceNumber() const {
    return sequence_number_;
  }

  void Uuid(const std::string& uuid) {
    uuid_ = uuid;
  }
  [[nodiscard]] const std::string& Uuid() const {
    return uuid_;
  }

  void GenerateJson();
  void GenerateProtobuf();


  void Body(const BodyList& body) {
    body_ = body;
  }
  [[nodiscard]] const BodyList& Body() const {
    return body_;
  }
  [[nodiscard]] BodyList& Body() {
    return body_;
  }

  IValue* CreateMetric(const std::string& name);
  void AddMetric(std::unique_ptr<IValue>& metric);
  [[nodiscard]] IValue* GetMetric(uint64_t alias);
  [[nodiscard]] const IValue* GetMetric(const std::string& name) const;
  [[nodiscard]] IValue* GetMetric(const std::string& name);
  [[nodiscard]] const MetricList& Metrics() const;
  void DeleteMetrics(const std::string& name);

  template<typename T>
  T GetValue(const std::string& name) const;

  template<typename T>
  void SetValue(const std::string& name, T value);
  std::string MakeJsonString() const;
 protected:
  std::atomic<uint64_t> timestamp_ = util::time::TimeStampToNs() / 1'000'000;
  mutable uint64_t sequence_number_ = 0;
  MetricList metric_list_;
  BodyList body_; ///< This is the payload data
 private:
  std::string uuid_;




};

template<typename T>
T IPayload::GetValue(const std::string &name) const {
  const IValue* metric = GetMetric(name);
  return metric != nullptr ? metric->Value<T>() : T {};
}

template<typename T>
void IPayload::SetValue(const std::string &name, T value) {
  if (IValue* metric = GetMetric(name); metric != nullptr ) {
    metric->Value(value);
  }
}

} // pub_sub
