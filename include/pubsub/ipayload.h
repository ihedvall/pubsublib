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
#include "pubsub/ivalue.h"

namespace pub_sub {

class IPayload {
 public:
  using BodyList = std::vector<uint8_t>;
  using MetricList = std::map<uint64_t, std::unique_ptr<IValue>>;

  void Timestamp(uint64_t ms_since_1970) {
    timestamp_ = ms_since_1970;
  }
  [[nodiscard]] uint64_t Timestamp() const {
    return timestamp_;
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

  void GenerateFullBody();


  void Body(const BodyList& body) {
    body_ = body;
  }
  [[nodiscard]] const BodyList& Body() const {
    return body_;
  }
  void AddMetric(std::unique_ptr<IValue>& metric);
  [[nodiscard]] IValue* GetMetric(uint64_t alias);
  [[nodiscard]] IValue* GetMetric(const std::string& name);
  [[nodiscard]] const MetricList& Metrics() const;
  void DeleteMetrics(const std::string& name);



 protected:

  std::atomic<uint64_t> timestamp_ = util::time::TimeStampToNs() / 1'000'000;
  mutable uint64_t sequence_number_ = 0;
  MetricList metric_list_;
  BodyList body_;
 private:
  std::string uuid_;


};

} // pub_sub
