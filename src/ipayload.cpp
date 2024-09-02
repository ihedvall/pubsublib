/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/ipayload.h"
#include "sparkplug_b_c_sharp.pb.h"
#include "payloadhelper.h"
#include "boost/json.hpp"


using namespace org::eclipse::tahu::protobuf;

namespace pub_sub {
void IPayload::AddMetric(std::unique_ptr<IValue>& metric) {
  const auto& name = metric->Name();
  metric_list_.insert({name, std::move(metric)});
}

IValue *IPayload::GetMetric(uint64_t alias) {
  auto itr = std::ranges::find_if(metric_list_, [&] (const auto& metric)->bool {
    return alias == metric.second->Alias();
  });
  return itr == metric_list_.end() ? nullptr : itr->second.get();
}

const IValue *IPayload::GetMetric(const std::string &name) const {
  const auto itr = metric_list_.find(name);
  return itr == metric_list_.cend() ? nullptr : itr->second.get();
}

IValue *IPayload::GetMetric(const std::string &name) {
  auto itr = metric_list_.find(name);
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
void IPayload::GenerateJson() {
  const std::string json = MakeJsonString();
  std::vector<uint8_t> body(json.size(), 0);
  for (size_t index = 0; index < json.size(); ++index) {
    body[index] = static_cast<uint8_t>(json[index]);
  }
  Body(body);
}

void IPayload::GenerateProtobuf() {
  Payload payload;
  PayloadHelper::PayloadToProtobuf(*this, payload);
  const size_t body_size = payload.ByteSizeLong();
  body_.resize(body_size);
  payload.SerializeToArray(body_.data(),static_cast<int>(body_.size()));
}


IValue *IPayload::CreateMetric(const std::string &name) {
  auto* exist = GetMetric(name);
  if (exist != nullptr) {
    return exist;
  }

  std::unique_ptr<IValue> metric = std::make_unique<IValue>(name);
  metric_list_.insert({name, std::move(metric)});
  return GetMetric(name);
}

std::string IPayload::MakeJsonString() const {
  boost::json::object obj;
  for (const auto& [name,metric] : metric_list_) {
    if (!metric || name.empty()) {
      continue;
    }
    if (metric->IsNull()) {
      obj[name] = nullptr;
      continue;
    }
    switch (metric->Type()) {
      case ValueType::Int8:
      case ValueType::Int16:
      case ValueType::Int32:
      case ValueType::Int64:
        obj[name] = metric->Value<int64_t>();
        break;

      case ValueType::UInt8:
      case ValueType::UInt16:
      case ValueType::UInt32:
      case ValueType::UInt64:
        obj[name] = metric->Value<uint64_t>();
        break;

      case ValueType::Float:
      case ValueType::Double:
        obj[name] = metric->Value<double>();
        break;

      case ValueType::Boolean:
        obj[name] = metric->Value<bool>();
        break;

      case ValueType::Text:
      case ValueType::String:
        obj[name] = metric->Value<std::string>();
        break;

      default:
        continue;
    }
  }
  return boost::json::serialize(obj);
}


} // pub_sub