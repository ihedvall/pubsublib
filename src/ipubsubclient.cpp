/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <util/stringutil.h>
#include "pubsub/ipubsubclient.h"
#include <util/ihwinfo.h>

#include "sparkplughost.h"

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
  std::scoped_lock list_lock(topic_mutex_);

  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && topic_name == topic->Topic();
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetITopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex_);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetTopicByMessageType(const std::string &message_type) {
  std::scoped_lock list_lock(topic_mutex_);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(message_type,topic->MessageType());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();

}
void IPubSubClient::DeleteTopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex_);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  if (itr != topic_list_.end()) {
    topic_list_.erase(itr);
  }
}

void IPubSubClient::AddSubscription(std::string topic_name) {
  const bool exist = std::any_of(subscription_list_.cbegin(), subscription_list_.cend(),
                                 [&] (const std::string& topic)->bool {
                                   return topic_name == topic;
                                 });
  if (!exist) {
    subscription_list_.emplace_back(std::move(topic_name));
  }
}

void IPubSubClient::AddSubscriptionFront(std::string topic_name) {
  const bool exist = std::any_of(subscription_list_.cbegin(), subscription_list_.cend(),
                                 [&] (const std::string& topic)->bool {
                                   return topic_name == topic;
                                 });
  if (!exist) {
    subscription_list_.emplace_front(std::move(topic_name));
  }
}

void IPubSubClient::DeleteSubscription(const std::string &topic_name) {
  auto itr = std::find_if(subscription_list_.begin(), subscription_list_.end(),
                          [&] (const std::string& topic)->bool {
                            return topic_name == topic;
                          });
  if (itr != subscription_list_.end())  {
    subscription_list_.erase(itr);
  }
}

const std::list<std::string> &IPubSubClient::Subscriptions() const {
  return subscription_list_;
}

void IPubSubClient::ScanRate(int64_t scan_rate) {
  scan_rate_ = scan_rate;
}

int64_t IPubSubClient::ScanRate() const {
  return scan_rate_;
}

IPubSubClient *IPubSubClient::CreateDevice(const std::string &) {
  return nullptr;
}

void IPubSubClient::DeleteDevice(const std::string &) {
}

IPubSubClient *IPubSubClient::GetDevice(const std::string &) {
  return nullptr;
}

const IPubSubClient *IPubSubClient::GetDevice(const std::string &) const {
  return nullptr;
}

std::string IPubSubClient::VersionAsString() const {
  switch (Version()) {
    case ProtocolVersion::Mqtt31:
      return "MQTT 3.1";

    case ProtocolVersion::Mqtt5:
      return "MQTT 5.0";

    default:
      break;
  }
  return "MQTT 3.1.1";
}

void IPubSubClient::PublishTopics() {
  std::scoped_lock lock(topic_mutex_);
  for (auto& topic : topic_list_) {
    if (!topic || !topic->Publish() || !topic->IsUpdated()) {
      continue;
    }
    if (topic->IsUpdated()) {
      topic->ResetUpdated();
      topic->DoPublish();
    }
  }
}

} // end namespace mqtt