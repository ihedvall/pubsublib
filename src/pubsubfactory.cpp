/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/pubsubfactory.h"
#include "mqttclient.h"
#include "detectbroker.h"

namespace pub_sub {

std::unique_ptr<IPubSubClient> PubSubFactory::CreatePubSubClient(PubSubType type) {
  std::unique_ptr<IPubSubClient> client;

  switch (type) {
    case PubSubType::Mqtt3Client: {
      auto mqtt_client = std::make_unique<MqttClient>();
      mqtt_client->Version(ProtocolVersion::Mqtt311);
      client = std::move(mqtt_client);
      break;
    }

    case PubSubType::DetectMqttBroker: {
      auto mqtt_client = std::make_unique<DetectBroker>();
      client = std::move(mqtt_client);
      break;
    }

    default:
      break;
  }
  return client;
}

std::shared_ptr<IValue> PubSubFactory::CreateValue(const std::string_view &name) {
  auto value = std::make_shared<IValue>(name);
  return value;
}

std::shared_ptr<IValue> PubSubFactory::CreateValue(const std::string &name) {
  auto value = std::make_shared<IValue>(name);
  return value;
}

} // pub_sub