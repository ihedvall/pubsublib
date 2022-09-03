/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/pubsubfactory.h"
#include "mqttclient.h"
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

    default:
      break;
  }
  return client;
}

} // pub_sub