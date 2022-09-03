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

ITopic *IPubSubClient::GetTopicByMessageType(const std::string& message_type) {
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

void IPubSubClient::ClearTopic() {
  std::scoped_lock list_lock(topic_mutex);
  topic_list_.clear();
}


} // end namespace mqtt