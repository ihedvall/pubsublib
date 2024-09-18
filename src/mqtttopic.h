/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <atomic>
#include "MQTTAsync.h"
#include "pubsub/itopic.h"


namespace pub_sub {

class MqttClient;
class MqttTopic : public ITopic {
 public:
  explicit MqttTopic(MqttClient& parent);
  MqttTopic() = delete;
  void DoPublish() override;
  void DoSubscribe() override;

  void ParsePayloadData() override;
 protected:

 private:
  MqttClient& parent_;

  std::vector<uint8_t> lrv_; ///< MQTT last reported value. Used as message buffer.
  std::atomic<bool> message_sent_ = true;
  std::atomic<bool> message_failed_ = false;
  static void OnSendFailure(void *context, MQTTAsync_failureData *response);
  static void OnSend(void *context, MQTTAsync_successData *response);
  static void OnSubscribeFailure(void *context, MQTTAsync_failureData *response);
  static void OnSubscribe(void *context, MQTTAsync_successData *response);
};

}