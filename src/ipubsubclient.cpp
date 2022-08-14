/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include <algorithm>
#include "pubsub/ipubsubclient.h"
#include "util/stringutil.h"
#include "mqttclient.h"

using namespace util::string;

namespace pub_sub {

ITopic *IPubSubClient::GetTopic(const std::string &topic_name) {
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && topic_name == topic->Topic();
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetITopic(const std::string &topic_name) {
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

std::unique_ptr<IPubSubClient> CreatePubSubClient(PubSubType type) {
  std::unique_ptr<IPubSubClient> client;

  switch (type) {
    case PubSubType::Mqtt3Client: {
      auto mqtt_client = std::make_unique<MqttClient>();
      mqtt_client->Version(ProtocolVersion::Mqtt311);
      client = std::move(mqtt_client);
      break;
    }

    default:
      break;
  }
  return client;
}
} // end namespace mqtt