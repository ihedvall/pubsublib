/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <string>
#include <set>
#include <atomic>
#include <memory>
#include <MQTTAsync.h>
#include <util/ilisten.h>
#include "pubsub/ipubsubclient.h"

namespace pub_sub {

class MqttClient : public IPubSubClient {
 public:
  MqttClient();
  ~MqttClient() override;
  bool Start() override;
  bool Stop() override;
  [[nodiscard]] bool IsConnected() const override;

  [[nodiscard]] ITopic* CreateTopic() override;
  [[nodiscard]] ITopic* AddValue(const std::shared_ptr<IValue>& value) override;
  [[nodiscard]] MQTTAsync Handle() const {
    return handle_;
  }
  [[nodiscard]] util::log::IListen& Listen() const {
    return *listen_;
  }

  [[nodiscard]] bool IsOnline() const override;
  [[nodiscard]] bool IsOffline() const override;
 protected:
  MQTTAsync handle_ = nullptr;
  std::unique_ptr<util::log::IListen> listen_;
  std::atomic<bool> disconnect_ready_ = false; ///< Indicate the the client has been disconnected.

  virtual bool SendConnect();
  virtual void DoConnect();

  void ConnectionLost(const std::string& cause);
  virtual void Message(const std::string& topic_name, const MQTTAsync_message& message);
  void DeliveryComplete(MQTTAsync_token token);
  void Connect(const MQTTAsync_successData* response);
  void ConnectFailure(MQTTAsync_failureData* response);
  void Disconnect(MQTTAsync_successData* response);
  void DisconnectFailure( MQTTAsync_failureData* response);

  static void OnConnectionLost(void *context, char *cause);
  static int OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message);
  static void OnDeliveryComplete(void *context, MQTTAsync_token token);
  static void OnConnect(void* context, MQTTAsync_successData* response);
  static void OnConnectFailure(void* context, MQTTAsync_failureData* response);
  static void OnDisconnect(void* context, MQTTAsync_successData* response);
  static void OnDisconnectFailure(void* context, MQTTAsync_failureData* response);


 private:
  void OnPublish(IValue& value);
};


} // end namespace
