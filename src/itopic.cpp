/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/itopic.h"
#include "util/stringutil.h"
#include "util/timestamp.h"

namespace pub_sub {

template<>
void ITopic::Payload(const std::vector<uint8_t>& payload)
{  if (content_type_.empty()) {
    content_type_ = "application/octet-stream";
  }
  // 1. Lock the topic data from updating.
  // 2. Check if the new payload differs from previous
  // 3. If differs publish the payload data.
  std::lock_guard lock(topic_mutex_);
  if (payload != payload_ || update_counter_ == 0) {
    UpdatePayload(payload);
  }
}

template<>
void ITopic::Payload(const bool& payload)
{
  std::string temp = payload ? "1": "0";
  Payload(temp);
}

template<>
void ITopic::Payload(const float& payload) {
  Payload(util::string::FloatToString(payload));
}

template<>
void ITopic::Payload(const double& payload) {
  Payload(util::string::DoubleToString(payload));
}

template<>
std::vector<uint8_t> ITopic::Payload() const {
  std::lock_guard lock(topic_mutex_);
  return payload_;
}

template<>
bool ITopic::Payload() const {
  std::lock_guard lock(topic_mutex_);
  if (payload_.empty()) {
    return false;
  }
  switch (payload_[0]) {
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
  timestamp_ = util::time::TimeStampToNs();
  payload_ = payload;
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

} // end namespace util::mqtt