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
#include <mutex>
#include "pubsub/itopic.h"

namespace pub_sub {

enum class TransportLayer: int {
  MqttTcp,
  MqttWebSocket,
  MqttTcpTls,
  MqttWebSocketTls,
};

enum class ProtocolVersion : int {
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

  void GroupId(const std::string& group_id) {
    group_id_ = group_id;
  }
  [[nodiscard]] const std::string& GroupId() const {
    return group_id_;
  }

  void NodeId(const std::string& node_id) {
    node_id_ = node_id;
  }
  [[nodiscard]] const std::string& NodeId() const {
    return node_id_;
  }

  void Transport(TransportLayer transport) {
    transport_ = transport;
  }
  [[nodiscard]] TransportLayer Transport() const {
    return transport_;
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
  ITopic* GetTopic(const std::string& topic_name);
  ITopic* GetITopic(const std::string& topic_name);
  ITopic* GetTopicByMessageType(const std::string& message_type);
  void DeleteTopic(const std::string& topic_name);
  void ClearTopic();

  virtual bool Start() = 0; ///< Connects to the MQTT server.
  virtual bool Stop() = 0; ///< Disconnect from the MQTT server.
  [[nodiscard]] virtual bool IsConnected() const = 0;
 protected:
  ProtocolVersion version_ = ProtocolVersion::Mqtt3; ///< Using version 3.1.1 as default.
  TransportLayer transport_ = TransportLayer::MqttTcp; ///< Defines the underlying transport protocol and encryption.
  std::string broker_ = "127.0.0.1"; ///< Address to the MQTT server (broker).
  uint16_t port_ = 1883; ///< The MQTT broker server port.

  std::string client_id_;
  std::string group_id_;
  std::string node_id_;

  std::recursive_mutex topic_mutex; ///< Thread protection of the topic list
  TopicList topic_list_; ///< List of topics.
};




} // end namespace
