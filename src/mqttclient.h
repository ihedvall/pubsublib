/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <string>
#include <set>
#include <MQTTAsync.h>
#include "pubsub/ipubsubclient.h"
#include "util/ilisten.h"

namespace pub_sub {

class MqttClient : public IPubSubClient {
 public:
  MqttClient();
  ~MqttClient() override;
  void Start() override;
  void Stop() override;

  [[nodiscard]] ITopic* CreateTopic() override;

  [[nodiscard]] MQTTAsync Handle() const {
    return handle_;
  }
  [[nodiscard]] util::log::IListen& Listen() const {
    return *listen_;
  }
  bool FetchTopics(const std::string &search_topic, std::set<std::string>& topic_list) override;

 protected:
  void DoConnect();
 private:
  MQTTAsync handle_ = nullptr;
  std::string connect_string_;
  std::unique_ptr<util::log::IListen> listen_;

  std::atomic<bool> connected_ = false; ///< Indicate the the client is connected.
  std::atomic<bool> disconnect_ready_ = false; ///< Indicate the the client has been disconnected.
  bool wildcard_search_ = false; // Just an indication that this is a wildcard search of topics
  std::atomic<uint64_t> wildcard_count_ = 0;
  std::set<std::string> wildcard_list_;

  static void OnConnectionLost(void *context, char *cause);
  static int OnMessageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);
  static void OnDeliveryComplete(void *context, MQTTAsync_token token);
  static void OnConnect(void* context, MQTTAsync_successData* response);
  static void OnConnectFailure(void* context,  MQTTAsync_failureData* response);
  static void OnDisconnect(void* context, MQTTAsync_successData* response);
  static void OnDisconnectFailure(void* context,  MQTTAsync_failureData* response);
};


} // end namespace
