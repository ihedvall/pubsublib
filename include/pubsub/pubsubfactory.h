/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "pubsub/metric.h"
#include "pubsub/ipubsubclient.h"

namespace workflow {
class ITaskFactory;
}

namespace pub_sub {

enum class PubSubType : int {
  Mqtt3Client = 0, ///< MQTT 3.11 client interface.
  Mqtt5Client = 1, ///< MQTT 5 client interface.
  SparkplugNode = 2, ///< Sparkplug Node interface.
  SparkplugHost = 3, ///< Sparkplug Host interface.
  KafkaClient = 4, ///< Kafka client.
  DetectMqttBroker = 5 ///< Specialized client that detect an MQTT broker
};

class PubSubFactory {
 public:
/** \brief Creates a Pub/Sub client interface.
 *
 *  Creates a pub/sub client. Currently, only MQTT/Sparkplug B clients are
 *  supported.
 *
 * @param type Type of publisher.
 * @return Smart pointer to a Pub/Sub client source.
 */
  static std::unique_ptr<IPubSubClient> CreatePubSubClient(PubSubType type);
  static workflow::ITaskFactory& GetWorkflowFactory();

  static std::shared_ptr<Metric> CreateMetric(const std::string_view& name);
  static std::shared_ptr<Metric> CreateMetric(const std::string& name);
};

} // pub_sub
