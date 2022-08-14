/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <functional>
#include <set>
#include "pubsub/itopic.h"

namespace pub_sub {

enum class PubSubType : int {
  Mqtt3Client = 0, ///< MQTT 3.11 client interface.
  Mqtt5Client = 1, ///< MQTT 5 client interface.
  SparkPlugClient = 2, ///< MQTT version 3.11 with SparkPlug interface.
  KafkaClient = 3, ///< Kafka client.
};
enum class ProtocolVersion {
  Mqtt3 = 3,
  Mqtt311 = 4,
  Mqtt5 = 5
};

class IPubSubClient {
 public:
  using TopicList = std::vector<std::unique_ptr<ITopic>>;

  IPubSubClient() = default;
  virtual ~IPubSubClient() = default;

  void ClientId(const std::string& client_id) {
    client_id_ = client_id;
  }
  [[nodiscard]] const std::string& ClientId() const {
    return client_id_;
  }

  void Broker(const std::string& address) {
    broker_ = address;
  }
  [[nodiscard]] const std::string& Broker() const {
    return broker_;
  }

  void Port(uint16_t port) {
    port_ = port;
  }
  [[nodiscard]] uint16_t Port() const {
    return port_;
  }

  void Version(ProtocolVersion version) {
    version_ = version;
  }
  [[nodiscard]] ProtocolVersion Version() const {
    return version_;
  }

  virtual ITopic* CreateTopic() = 0;
  virtual bool FetchTopics(const std::string& search_topic, std::set<std::string>& topic_list) = 0;

  ITopic* GetTopic(const std::string& topic);
  ITopic* GetITopic(const std::string& topic);
  [[nodiscard]] const TopicList& Topics() const {
    return topic_list_;
  }

  virtual void Start() = 0; ///< Connects to the MQTT server.
  virtual void Stop() = 0; ///< Disconnect from the MQTT server.
 protected:
  ProtocolVersion version_ = ProtocolVersion::Mqtt5; ///< Using version 5 as default.
  std::string broker_ = "127.0.0.1"; ///< Address to the MQTT server (broker).
  uint16_t port_ = 1883; ///< The MQTT broker server port.
  std::string client_id_;
  TopicList topic_list_; ///< List of topics.

};

/** \brief Creates a publisher client interface.
 *
 *  Creates a pre-defined publisher source. Currently on MQTT is available.
 *
 * @param type Type of publisher.
 * @return Smart pointer to a Pub/Sub client source.
 */
extern std::unique_ptr<IPubSubClient> CreatePubSubClient(PubSubType type);


} // end namespace
