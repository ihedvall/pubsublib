/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/ipayload.h"
#include "sparkplug_b.pb.h"
#include "payloadhelper.h"
using namespace org::eclipse::tahu::protobuf;

namespace pub_sub {
void IPayload::AddMetric(std::unique_ptr<IMetric>& metric) {
  const auto alias = metric->Alias();
  metric_list_.insert({alias, std::move(metric)});
}

IMetric *IPayload::GetMetric(uint64_t alias) {
  auto itr = metric_list_.find(alias);
  return itr == metric_list_.end() ? nullptr : itr->second.get();
}

IMetric *IPayload::GetMetric(const std::string &name) {
  auto itr = std::ranges::find_if(metric_list_, [&] (const auto& metric) {
    return util::string::IEquals(name, metric.second->Name());
  });
  return itr == metric_list_.end() ? nullptr : itr->second.get();
}

const IPayload::MetricList &IPayload::Metrics() const {
  return metric_list_;
}

void IPayload::DeleteMetrics(const std::string &name) {
  auto itr = std::ranges::find_if(metric_list_, [&] (const auto& metric) {
    return util::string::IEquals(name, metric.second->Name());
  });
  if (itr != metric_list_.end()) {
    metric_list_.erase(itr);
  };
}
void IPayload::GenerateFullBody() {
  Payload payload;
  PayloadHelper::PayloadToProtobuf(*this, payload);
  const size_t body_size = payload.ByteSizeLong();
  body_.clear();
  body_.resize(body_size);
  payload.SerializeToArray(body_.data(),static_cast<int>(body_.size()));
}

} // pub_sub