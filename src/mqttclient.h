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
#include "sparkplughelper.h"

namespace pub_sub {

class MqttClient : public IPubSubClient {

 public:

  MqttClient();
  ~MqttClient() override;

  bool Start() override;
  bool Stop() override;

  [[nodiscard]] bool IsConnected() const override;

  [[nodiscard]] ITopic* CreateTopic() override;
  [[nodiscard]] ITopic* AddMetric(const std::shared_ptr<Metric>& value) override;

  [[nodiscard]] MQTTAsync Handle() const {
    return handle_;
  }
  [[nodiscard]] util::log::IListen& Listen() const {
    return *listen_;
  }

  [[nodiscard]] bool IsOnline() const override;
  [[nodiscard]] bool IsOffline() const override;

 private:
  MQTTAsync handle_ = nullptr;
  std::unique_ptr<util::log::IListen> listen_;
  std::condition_variable client_event_; ///< Can be used to speed up the scanning of the thread
  std::mutex client_mutex_; ///< Used to wait for events
  std::thread work_thread_; ///< Handles the online connect and subscription

  enum class ClientState {
    Idle,             ///< Initial state, wait on in-service
    WaitOnConnect,    ///< Wait on connect
    Online,
    WaitOnDisconnect
  };
  std::atomic<ClientState> client_state_ = ClientState::Idle;
  std::atomic<bool> stop_client_task_ = true;
  uint64_t client_timer_ = SparkplugHelper::NowMs();

  std::atomic<bool> delivered_ = false;

  void ResetDelivered() { delivered_ = false;}
  void SetDelivered() { delivered_ = true; }
  [[nodiscard]] bool IsDelivered() const { return delivered_; }

  bool SendConnect();
  bool SendDisconnect();
  void StartSubscription();


  void DoIdle();
  void DoWaitOnConnect();
  void DoOnline();
  void DoWaitOnDisconnect();


  void ClientTask();
  bool CreateClient();

  void ConnectionLost(const std::string& cause);
  void Message(const std::string& topic_name, const MQTTAsync_message& message);
  void DeliveryComplete(MQTTAsync_token token);
  void Connect(const MQTTAsync_successData& response);
  void ConnectFailure(const MQTTAsync_failureData* response);
  void Connect5(const MQTTAsync_successData5& response);
  void ConnectFailure5(const MQTTAsync_failureData5* response);
  void SubscribeFailure(const MQTTAsync_failureData& response);
  void SubscribeFailure5(const MQTTAsync_failureData5& response);
  void Disconnect(const MQTTAsync_successData* response);
  void DisconnectFailure(const MQTTAsync_failureData* response);
  void Disconnect5(const MQTTAsync_successData5* response);
  void DisconnectFailure5(const MQTTAsync_failureData5* response);

  static void OnConnectionLost(void *context, char *cause);
  static int OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message);
  static void OnDeliveryComplete(void *context, MQTTAsync_token token);
  static void OnConnect(void* context, MQTTAsync_successData* response);
  static void OnConnectFailure(void* context, MQTTAsync_failureData* response);
  static void OnConnect5(void* context, MQTTAsync_successData5* response);
  static void OnConnectFailure5(void* context, MQTTAsync_failureData5* response);

  static void OnSubscribeFailure(void *context, MQTTAsync_failureData *response);
  static void OnSubscribeFailure5(void *context, MQTTAsync_failureData5 *response);
  static void OnDisconnect(void* context, MQTTAsync_successData* response);
  static void OnDisconnectFailure(void* context, MQTTAsync_failureData* response);
  static void OnDisconnect5(void* context, MQTTAsync_successData5* response);
  static void OnDisconnectFailure5(void* context, MQTTAsync_failureData5* response);
};


} // end namespace
