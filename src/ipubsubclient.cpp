/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <util/stringutil.h>
#include "pubsub/ipubsubclient.h"
#include <util/ihwinfo.h>

using namespace util::string;
using namespace util::hw_info;

namespace pub_sub {
IPubSubClient::IPubSubClient()
: hardware_make_(IHwInfo::CpuVendor()),
  hardware_model_(IHwInfo::CpuModel()),
  operating_system_(IHwInfo::OsName()),
  os_version_(IHwInfo::OsKernel()) {

}

ITopic *IPubSubClient::GetTopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex);

  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && topic_name == topic->Topic();
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetITopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetTopicByMessageType(const std::string &message_type) {
  std::scoped_lock list_lock(topic_mutex);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(message_type,topic->MessageType());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();

}
void IPubSubClient::DeleteTopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  if (itr != topic_list_.end()) {
    topic_list_.erase(itr);
  }
}

void IPubSubClient::ClearTopicList() {
  std::scoped_lock list_lock(topic_mutex);
  topic_list_.clear();
}

bool IPubSubClient::IsFaulty() const {
  std::scoped_lock list_lock(topic_mutex);
  return faulty_;
}

void IPubSubClient::SetFaulty(bool faulty, const std::string &error_text) {
  std::scoped_lock list_lock(topic_mutex);
  faulty_ = faulty;
  last_error_ = error_text;
}

int IPubSubClient::GetUniqueToken() {
  return unique_token++;
}

void IPubSubClient::AddSubscriptionByTopic(const std::string &topic_name) {
  const bool exist = std::any_of(subscription_list_.cbegin(), subscription_list_.cend(),
                                 [&] (const std::string& topic)->bool {
                                   return topic_name == topic;
                                 });
  if (!exist) {
    subscription_list_.emplace_back(topic_name);
  }
}

void IPubSubClient::DeleteSubscriptionByTopic(const std::string &topic_name) {
  auto itr = std::find_if(subscription_list_.begin(), subscription_list_.end(),
                          [&] (const std::string& topic)->bool {
                            return topic_name == topic;
                          });
  if (itr != subscription_list_.end())  {
    subscription_list_.erase(itr);
  }
}

const std::vector<std::string> &IPubSubClient::Subscriptions() const {
  return subscription_list_;
}

void IPubSubClient::ScanRate(int64_t scan_rate) {
  scan_rate_ = scan_rate;
}

int64_t IPubSubClient::ScanRate() const {
  return scan_rate_;
}

IPubSubClient *IPubSubClient::CreateDevice(const std::string&) {
  // Only Sparkplug nodes can create devices.
  return nullptr;
}

void IPubSubClient::DeleteDevice(const std::string&) {

}

IPubSubClient *IPubSubClient::GetDevice(const std::string&) {
  return nullptr;
}

const IPubSubClient *IPubSubClient::GetDevice(const std::string&) const {
  return nullptr;
}

} // end namespace mqtt