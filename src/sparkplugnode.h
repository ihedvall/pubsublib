/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <MQTTAsync.h>
#include <util/ilisten.h>
#include "pubsub/ipubsubclient.h"
#include "sparkplughelper.h"


namespace pub_sub {

class SparkplugDevice;

class SparkplugNode : public IPubSubClient {
 public:
  SparkplugNode();
  ~SparkplugNode() override;

  bool Start() override;
  bool Stop() override;

  [[nodiscard]] bool IsOnline() const override;
  [[nodiscard]] bool IsOffline() const override;

  ITopic* CreateTopic() override;
  ITopic* AddMetric(const std::shared_ptr<IMetric>& value) override;
  [[nodiscard]] bool IsConnected() const override;

  [[nodiscard]] const std::string& ServerUri() const { return server_uri_; }
  [[nodiscard]] int ServerVersion() const { return server_version_; }
  [[nodiscard]] int ServerSession() const { return server_session_; }

  MQTTAsync& Handle() { return handle_; }
  util::log::IListen* Listen() { return listen_.get(); }

  void ResetDelivered() { delivered_ = false;}
  void SetDelivered() { delivered_ = true; }
  bool IsDelivered();

  [[nodiscard]] IPubSubClient* CreateDevice(const std::string& device_name) override;
  void DeleteDevice(const std::string& device_name) override;

  [[nodiscard]] IPubSubClient* GetDevice(const std::string& device_name) override;
  [[nodiscard]] const IPubSubClient* GetDevice(const std::string& device_name) const override;

 protected:
  MQTTAsync handle_ = nullptr;
  std::unique_ptr<util::log::IListen> listen_;
  std::condition_variable node_event_;
  std::mutex node_mutex_;
  std::thread work_thread_; ///< Handles the online connect and subscription

  std::atomic<bool> delivered_ = false;

  std::string server_uri_;
  int server_version_ = 0;
  int server_session_ = -1; ///< Indicate if connected -1 = Unknown, 0/1 = Not Connected/Connected

  virtual bool SendConnect();
  void StartSubscription();
  void SendDisconnect();

  void Connect(const MQTTAsync_successData& response);
  void ConnectFailure(const MQTTAsync_failureData& response);
  void ConnectionLost(const std::string& reason);
  void Message(const std::string& topic_name, const MQTTAsync_message& message);
  void DeliveryComplete(MQTTAsync_token token);
  void SubscribeFailure(MQTTAsync_failureData& response);
  void Disconnect(MQTTAsync_successData& response);
  void DisconnectFailure( MQTTAsync_failureData& response);

  static void OnConnectionLost(void *context, char *cause);
  static int OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message);
  static void OnDeliveryComplete(void *context, MQTTAsync_token token);
  static void OnConnect(void* context, MQTTAsync_successData* response);
  static void OnConnectFailure(void* context, MQTTAsync_failureData* response);
  static void OnSubscribeFailure(void *context, MQTTAsync_failureData *response);
  static void OnSubscribe(void *context, MQTTAsync_successData *response);
  static void OnDisconnect(void* context, MQTTAsync_successData* response);
  static void OnDisconnectFailure(void* context, MQTTAsync_failureData* response);
 private:
  using DeviceList =  std::map<std::string, std::unique_ptr<SparkplugDevice>, util::string::IgnoreCase>;

  uint64_t bd_sequence_number_ = 0; ///< Birth/Death sequence number

  enum class NodeState {
    Idle,             ///< Initial state, wait on in-service
    WaitOnConnect,    ///< Wait on connect
    Online,
    WaitOnDisconnect
  };

  std::atomic<NodeState> node_state_ = NodeState::Idle;
  std::atomic<bool> stop_node_task_ = true;
  uint64_t node_timer_ = SparkplugHelper::NowMs();
  DeviceList device_list_; ///< Sparkplug devices in this node


  bool CreateNode();
  void CreateNodeDeathTopic();
  void CreateNodeBirthTopic();
  void PublishNodeBirth();
  void PublishNodeDeath();
  void PollDevices();

  void NodeTask();
  void DoIdle();
  void DoWaitOnConnect();
  void DoOnline();
  void DoWaitOnDisconnect();

};

} // pub_sub
