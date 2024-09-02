/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <util/stringutil.h>
#include "pubsub/ipubsubclient.h"

using namespace util::string;

namespace pub_sub {

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

ITopic *IPubSubClient::GetTopicByMessageType(const std::string& message_type, bool publisher) {
  std::scoped_lock list_lock(topic_mutex);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(message_type,topic->MessageType()) && topic->Publish() == publisher;
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

} // end namespace mqtt