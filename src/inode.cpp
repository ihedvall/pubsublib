/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/inode.h"

namespace pub_sub {
void INode::AddSubscriptionByTopic(const std::string &topic_name) {
  const bool exist = std::any_of(subscription_list_.cbegin(), subscription_list_.cend(),
                                 [&] (const std::string& topic)->bool {
    return topic_name == topic;
  });
  if (!exist) {
    subscription_list_.emplace_back(topic_name);
  }
}

void INode::DeleteSubscriptionByTopic(const std::string &topic_name) {
  auto itr = std::find_if(subscription_list_.begin(), subscription_list_.end(),
                                 [&] (const std::string& topic)->bool {
                                   return topic_name == topic;
                                 });
  if (itr != subscription_list_.end())  {
    subscription_list_.erase(itr);
  }
}

const std::vector<std::string> &INode::Subscriptions() const {
  return subscription_list_;
}

} // pub_sub