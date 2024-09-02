/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <string>
#include <vector>
#include "pubsub/ipubsubclient.h"
namespace pub_sub {
    class INode : public IPubSubClient {
    public:
      void AddSubscriptionByTopic(const std::string& topic_name);
      void DeleteSubscriptionByTopic(const std::string& topic_name);
      const std::vector<std::string>& Subscriptions() const;
    protected:
      std::vector<std::string> subscription_list_;

    };

} // pub_sub

