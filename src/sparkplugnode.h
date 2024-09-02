/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <atomic>
#include <MQTTAsync.h>
#include <util/ilisten.h>
#include "pubsub/inode.h"

namespace pub_sub {

class SparkplugNode : public INode {
 public:
  SparkplugNode();
  ~SparkplugNode() override;

  bool Start() override;
  bool Stop() override;

  [[nodiscard]] bool IsOnline() const override;
  [[nodiscard]] bool IsOffline() const override;

  ITopic* CreateTopic() override;
  ITopic* AddValue(const std::shared_ptr<IValue>& value) override;
  [[nodiscard]] bool IsConnected() const override;
  [[nodiscard]] const std::string& ServerUri() const { return server_uri_; }
  [[nodiscard]] int ServerVersion() const { return server_version_; }
  [[nodiscard]] int ServerSession() const { return server_session_; }

  MQTTAsync& Handle() { return handle_; }
  util::log::IListen* Listen() { return listen_.get(); }

  void NextToken(int next_token) { next_token_ = next_token;}
  bool IsDelivered();

 protected:
  MQTTAsync handle_ = nullptr;
  std::unique_ptr<util::log::IListen> listen_;
  std::condition_variable host_event_;

  std::atomic<MQTTAsync_token> next_token_ = 0;
  std::atomic<MQTTAsync_token> last_token_ = 0;

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

  uint64_t birth_death_sequence_number = 0;
  ITopic* CreateNodeDeathTopic();
  ITopic* CreateNodeBirthTopic();


};

} // pub_sub
