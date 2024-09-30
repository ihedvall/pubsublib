/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/payload.h"
#include "sparkplug_b.pb.h"
#include "payloadhelper.h"
#include "boost/json.hpp"
#include "util/logstream.h"

using namespace org::eclipse::tahu::protobuf;
using namespace boost::json;

namespace pub_sub {

void Payload::Timestamp(uint64_t ms_since_1970, bool set_metrics) {
  timestamp_ = ms_since_1970;
  // Both the payload and its metric have timestamps so the below
  // lines should not happen with Sparkplug B.
  auto timestamp = GetMetric("timestamp");
  if (timestamp) {
    timestamp->Value(ms_since_1970);
  }

  if (!set_metrics) {
    return;
  }
  std::scoped_lock lock(payload_mutex_);
  for ( auto& [name, metric] : metric_list_) {
    if (metric) {
      metric->Timestamp(ms_since_1970);
    }
  }
}

uint64_t Payload::Timestamp() const {
  const auto timestamp = GetMetric("timestamp");
  return timestamp ? timestamp->Value<uint64_t>() : timestamp_.load();
}

void Payload::Uuid(const std::string &uuid) {
  std::scoped_lock lock(payload_mutex_);
  uuid_ = uuid;
}

std::string Payload::Uuid() const {
  std::scoped_lock lock(payload_mutex_);
  return uuid_;
}

std::shared_ptr<Metric> Payload::GetMetric(uint64_t alias) const {
  std::scoped_lock lock(payload_mutex_);
  auto itr = std::find_if(metric_list_.begin(), metric_list_.end(),
                          [&] (const auto& metric)->bool {
    return alias == metric.second->Alias();
  });
  return itr == metric_list_.end() ? std::shared_ptr<Metric>() : itr->second;
}


std::shared_ptr<Metric> Payload::GetMetric(const std::string &name) const {
  std::scoped_lock lock(payload_mutex_);
  auto itr = metric_list_.find(name);
  return itr == metric_list_.cend() ? std::shared_ptr<Metric>() : itr->second;
}

const Payload::MetricList &Payload::Metrics() const {
  return metric_list_;
}

void Payload::DeleteMetrics(const std::string &name) {
  std::scoped_lock lock(payload_mutex_);
  auto itr = std::ranges::find_if(metric_list_, [&] (const auto& metric) {
    return util::string::IEquals(name, metric.second->Name());
  });
  if (itr != metric_list_.end()) {
    metric_list_.erase(itr);
  };
}

void Payload::GenerateText() {
  const std::string text = MakeString();
  StringToBody(text);
}
void Payload::GenerateJson() {
  const std::string json = MakeJsonString();
  StringToBody(json);
}

void Payload::GenerateProtobuf() {
  PayloadHelper helper(*this);
  std::scoped_lock lock(payload_mutex_);
  helper.WriteProtobuf();
}


std::shared_ptr<Metric> Payload::CreateMetric(const std::string &name) {
  auto exist = GetMetric(name);
  if (exist) {
    return exist;
  }
  {
    std::scoped_lock lock(payload_mutex_);
    auto metric = std::make_shared<Metric>(name);
    metric_list_.insert({name, std::move(metric)});
  }
  return GetMetric(name);
}

 void Payload::AddMetric(const std::shared_ptr<Metric>& metric) {
  if (!metric || metric->Name().empty()) {
    LOG_ERROR() << "Metric must have a name.";
    return;
  }
  auto exist = GetMetric(metric->Name());
  if (exist) {
    LOG_INFO() << "Tried to add an existing metric. Existing: " << exist->Name()
      << ", New: " << metric->Name();
    return;
  }
  {
    std::scoped_lock lock(payload_mutex_);
    auto new_metric = metric;
    metric_list_.insert({new_metric->Name(), std::move(new_metric)});
  }
}

std::string Payload::MakeJsonString() const {
  boost::json::object obj;
  std::scoped_lock lock(payload_mutex_);
  for (const auto& [name,metric] : metric_list_) {
    if (!metric || name.empty()) {
      continue;
    }
    if (metric->IsNull()) {
      obj[name] = nullptr;
      continue;
    }
    switch (metric->Type()) {
      case MetricType::Int8:
      case MetricType::Int16:
      case MetricType::Int32:
      case MetricType::Int64:
        obj[name] = metric->Value<int64_t>();
        break;

      case MetricType::UInt8:
      case MetricType::UInt16:
      case MetricType::UInt32:
      case MetricType::UInt64:
        obj[name] = metric->Value<uint64_t>();
        break;

      case MetricType::Float:
      case MetricType::Double:
        obj[name] = metric->Value<double>();
        break;

      case MetricType::Boolean:
        obj[name] = metric->Value<bool>();
        break;

      case MetricType::Text:
      case MetricType::String:
        obj[name] = metric->Value<std::string>();
        break;

      default:
        continue;
    }
  }
  return boost::json::serialize(obj);
}

std::string Payload::MakeString() const {
  std::ostringstream temp;
  bool first = true;
  std::scoped_lock lock(payload_mutex_);
  for (const auto& [name,metric] : metric_list_) {
    if (!metric ) {
      continue;
    }
    if (!first) {
      temp << ";";
    }
    first = false;
    if (metric->IsNull()) {
      temp << "*";
    } else {
      temp << metric->Value<std::string>();
    }
  }

  return temp.str();
}

std::string Payload::BodyToString() const {
  std::ostringstream temp;

  for (uint8_t data : body_) {
    if (data == 0) {
      break;
    }
    temp << static_cast<char>(data);
  }
  return temp.str();
}

void Payload::StringToBody(const std::string &body_text) {
  try {
    body_.resize(body_text.size(), 0);
    memcpy(body_.data(), body_text.data(), body_.size());
  } catch(const std::exception& err) {
    LOG_ERROR() << "String to body failed. Err: " << err.what();
  }
}

void Payload::ParseSparkplugJson(bool create_metrics) {
  try {
    const auto json = BodyToString();
    const auto json_val = parse(json);
    const auto &json_obj = json_val.get_object();
    for (const auto& [key, val] : json_obj) {
      if (key.empty()) {
        continue;
      }
      auto metric = GetMetric(key);
      if (!metric && !create_metrics) {
        continue;
      }
      if (!metric) {
        metric = CreateMetric(key);
        if (!metric) {
          continue;
        }
        switch (val.kind()) {
          case kind::bool_:
            metric->Type(MetricType::Boolean);
            break;

          case kind::int64:
            metric->Type(MetricType::Int64);
            break;

          case kind::uint64:
            metric->Type(MetricType::UInt64);
            break;

          case kind::double_:
            metric->Type(MetricType::Double);
            break;

          case kind::string:
          case kind::null:
          default:
            metric->Type(MetricType::String);
            break;
        }
      } // if metric == nullptr

      // Update the value

      switch (val.kind()) {
        case kind::bool_:
          metric->Value(val.get_bool());
          metric->IsNull(false);
          break;

        case kind::int64:
          metric->Value(val.get_int64());
          metric->IsNull(false);
          break;

        case kind::uint64:
          metric->Value(val.get_uint64());
          metric->IsNull(false);
          break;

        case kind::double_:
          metric->Value(val.get_double());
          metric->IsNull(false);
          break;

        case kind::string:
          metric->Value(std::string(val.get_string()));
          metric->IsNull(false);
          break;

        case kind::null:
          metric->IsNull(true);
          break;

        default:
          metric->Type(MetricType::String);
          break;
      }
    } // end for loop
  } catch( const std::exception& err) {
    LOG_ERROR() << "JSON parser fail. Error: " << err.what();
  }
}

void Payload::ParseText(bool create_metrics) {
  try {
    const auto text = BodyToString();
    if (metric_list_.empty()) {
      auto metric = CreateMetric("Value");
      if (metric) {
        metric->Type(MetricType::String);
        metric->Value(text);
      }

    } else {
      auto &metric = metric_list_.begin()->second;
      if (metric) {
        metric->Value(text);
      }
    }
  } catch (const std::exception& ) {}
}

void Payload::ParseSparkplugProtobuf(bool create_metrics) {
  PayloadHelper helper(*this);
  helper.CreateMetrics(create_metrics);
  std::scoped_lock lock(payload_mutex_);
  helper.ParseProtobuf();
}

bool Payload::IsUpdated() const {
  std::scoped_lock lock(payload_mutex_);
  for (const auto&[name,metric] : metric_list_) {
    if (!metric) {
      continue;
    }
    if (metric->IsUpdated()) {
      return true;
    }
  }
  return false;
}

} // pub_sub