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

 protected:

 private:
  MqttClient& parent_;

  static void OnSendFailure(void *context, MQTTAsync_failureData *response);
  static void OnSendFailure5(void *context, MQTTAsync_failureData5 *response);

};

}