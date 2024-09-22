/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/pubsubfactory.h"
#include "mqttclient.h"
#include "detectbroker.h"
#include "sparkplugnode.h"
#include "sparkplughost.h"

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

    case PubSubType::SparkplugNode: {
      auto node = std::make_unique<SparkplugNode>();
      client = std::move(node);
      break;
    }

    case PubSubType::SparkplugHost: {
        auto host = std::make_unique<SparkplugHost>();
        client = std::move(host);
        break;
      }

    default:
      break;
  }
  return client;
}

std::shared_ptr<Metric> PubSubFactory::CreateMetric(const std::string_view &name) {
  auto value = std::make_shared<Metric>(name);
  return value;
}

std::shared_ptr<Metric> PubSubFactory::CreateMetric(const std::string &name) {
  auto value = std::make_shared<Metric>(name);
  return value;
}

} // pub_sub