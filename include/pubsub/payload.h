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
//#include <util/timestamp.h>
#include <util/stringutil.h>
#include "pubsub/metric.h"

namespace pub_sub {

class Payload {
 public:
  using BodyList = std::vector<uint8_t>;
  /** \brief Metric/Value list that is sorted on names.
   *
   * List of metrics also known as values. The list is sorted on names
   * and it ignore case. Although case sensitive names are valid, it's
   * a really bad design that causes many issues.
   */
  using MetricList = std::map<std::string, std::shared_ptr<Metric>,
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
  void Timestamp(uint64_t ms_since_1970, bool set_metrics = false);

  [[nodiscard]] uint64_t Timestamp() const;

  void SequenceNumber(uint64_t seq_no) const {
    sequence_number_ = seq_no;
  }
  [[nodiscard]] uint64_t SequenceNumber() const {
    return sequence_number_;
  }

  void Uuid(const std::string& uuid);
  [[nodiscard]] std::string Uuid() const;

  void GenerateJson();
  void GenerateProtobuf();
  void ParseSparkplugJson(bool create_metrics);
  void ParseSparkplugProtobuf(bool create_metrics);

  void Body(const BodyList& body) {
    body_ = body;
  }
  [[nodiscard]] const BodyList& Body() const {
    return body_;
  }
  [[nodiscard]] BodyList& Body() {
    return body_;
  }
  void StringToBody(const std::string& body_text);
  [[nodiscard]] std::string BodyToString() const;

  std::shared_ptr<Metric> CreateMetric(const std::string& name);
  void AddMetric(const std::shared_ptr<Metric>& metric);

  [[nodiscard]] std::shared_ptr<Metric> GetMetric(uint64_t alias) const;
  [[nodiscard]] std::shared_ptr<Metric> GetMetric(const std::string& name) const;

  [[nodiscard]] const MetricList& Metrics() const;
  void DeleteMetrics(const std::string& name);

  template<typename T>
  T GetValue(const std::string& name) const;

  template<typename T>
  void SetValue(const std::string& name, T value);
  std::string MakeJsonString() const;

 private:
  std::string uuid_;
  mutable std::recursive_mutex payload_mutex_;
  std::atomic<uint64_t> timestamp_ = 0;
  mutable std::atomic<uint64_t> sequence_number_ = 0;
  MetricList metric_list_;
  BodyList body_; ///< This is the payload data
};

template<typename T>
T Payload::GetValue(const std::string &name) const {
  auto metric = GetMetric(name);
  return metric ? metric->Value<T>() : T {};
}

template<typename T>
void Payload::SetValue(const std::string &name, T value) {
  if (auto metric = GetMetric(name); metric ) {
    metric->Value(value);
  }
}

} // pub_sub
