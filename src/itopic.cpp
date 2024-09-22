/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "util/stringutil.h"
#include "util/timestamp.h"
#include "pubsub/itopic.h"
namespace {
constexpr std::string_view kSparkplugNamespace = "spBv1.0";
}
namespace pub_sub {

void ITopic::Topic(const std::string &topic) {
  topic_ = topic;

  size_t level = 0;
  std::ostringstream temp;
  for (const char in_char : topic_) {
    if (in_char == '/') {
      AssignLevelName(level,temp.str());
      temp.str({});
      temp.clear();
      ++level;
    } else {
      temp << in_char;
    }
  }
  if (level > 0 && !temp.str().empty()) {
    AssignLevelName(level,temp.str());
  }

  // Handle special case of STATE message
  if (Namespace() == kSparkplugNamespace && GroupId() == "STATE") {
    NodeId(MessageType());
    MessageType(GroupId());
    GroupId("");
  }
}

const std::string &ITopic::Topic() const {
  if (topic_.empty()) {

    // Assume that the user uses the sparkplug namespace for topics.
    std::ostringstream temp;
    auto add_topic_part = [&] (const std::string& topic_part) {
      if (topic_part.empty()) {
        return;
      }
      if (!temp.str().empty()) {
        temp << "/";
      }
      temp << topic_part;
    };

    add_topic_part(name_space_);
    add_topic_part(group_id_);
    add_topic_part(message_type_);
    add_topic_part(node_id_);
    add_topic_part(device_id_);
    topic_ = temp.str();
  }
  return topic_;
}

template<>
void ITopic::PayloadBody(const std::vector<uint8_t>& payload)
{  if (content_type_.empty()) {
    content_type_ = "application/octet-stream";
  }
  // 1. Lock the topic data from updating.
  // 2. Check if the new payload differs from previous
  // 3. If differs publish the payload data.
  std::lock_guard lock(topic_mutex_);
  if (payload != payload_.Body() || update_counter_ == 0) {
    UpdatePayload(payload);
  }
}

template<>
void ITopic::PayloadBody(const bool& payload)
{
  const std::string temp = payload ? "1": "0";
  payload_.StringToBody(temp);
}

template<>
void ITopic::PayloadBody(const float& payload) {
  payload_.StringToBody(util::string::FloatToString(payload));
}

template<>
void ITopic::PayloadBody(const double& payload) {
  payload_.StringToBody(util::string::DoubleToString(payload));
}

template<>
std::vector<uint8_t> ITopic::PayloadBody() const {
  std::lock_guard lock(topic_mutex_);
  return payload_.Body();
}

template<>
bool ITopic::PayloadBody() const {
  std::lock_guard lock(topic_mutex_);
  const auto& body = payload_.Body();
  if (body.empty()) {
    return false;
  }
  switch (body[0]) {
    case 1:
    case '1':
    case 'T':
    case 't':
    case 'Y':
    case 'y':
      return true;
    default:
      break;
  }
  return false;
}

void ITopic::UpdatePayload(const std::vector<uint8_t> &payload) {
  payload_.Body(payload);
  updated_ = true;
  ++update_counter_;
  if (publish_) {
    DoPublish();
  }
}

bool ITopic::Updated() const {
  std::lock_guard lock(topic_mutex_);
  return updated_;
}

bool ITopic::IsWildcard() const {
  return strchr(topic_.c_str(), '+') != nullptr || strchr(topic_.c_str(), '#') != nullptr;
}

void ITopic::AssignLevelName(size_t level, const std::string &name) {
  switch (level) {
    case 0:
      if (name_space_.empty()) {
        name_space_ = name;
      }
      break;

    case 1:
      if (group_id_.empty()) {
        group_id_ = name;
      }
      break;

    case 2:
      if (message_type_.empty()) {
        message_type_ = name;
      }
      break;

    case 3:
      if (node_id_.empty()) {
        node_id_ = name;
      }
      break;

    case 4:
      if (device_id_.empty()) {
        device_id_ = name;
      }
      break;

    default:
      break;
  }
}

std::shared_ptr<Metric> ITopic::CreateMetric(const std::string &name) {
  return payload_.CreateMetric(name);
}

std::shared_ptr<Metric> ITopic::GetMetric(const std::string &name) const {
  return payload_.GetMetric(name);
}

void ITopic::SetAllMetricsInvalid() {
  auto& payload = GetPayload();
  std::scoped_lock lock(topic_mutex_);
  for ( const auto& [name, metric] : payload.Metrics() ) {
    if (metric) {
      metric->IsValid(false);
    }
  }
}

} // end namespace util::mqtt