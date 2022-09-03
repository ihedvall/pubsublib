/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "pubsub/ipubsubclient.h"

namespace pub_sub {

enum class PubSubType : int {
  Mqtt3Client = 0, ///< MQTT 3.11 client interface.
  Mqtt5Client = 1, ///< MQTT 5 client interface.
  SparkPlugNode = 2, ///< SparkPlug Edge of Node.
  SparkPlugClient = 3, ///< MQTT version 3.11 with SparkPlug interface.
  KafkaClient = 4, ///< Kafka client.
};

class PubSubFactory {
 public:
/** \brief Creates a publisher client interface.
 *
 *  Creates a pre-defined publisher source. Currently on MQTT is available.
 *
 * @param type Type of publisher.
 * @return Smart pointer to a Pub/Sub client source.
 */
  static std::unique_ptr<IPubSubClient> CreatePubSubClient(PubSubType type);
};

} // pub_sub
