/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "pubsub/itopic.h"
#include "MQTTAsync.h"

namespace pub_sub {

class SparkplugNode;
class SparkplugTopic : public ITopic {
 public:
  explicit SparkplugTopic(SparkplugNode& parent);
  SparkplugTopic() = delete;

  void DoPublish() override;
  void DoSubscribe() override;

  void ParsePayloadData() override;
 private:
  SparkplugNode& parent_;

  [[nodiscard]] bool IsValidMessageType() const;
  [[nodiscard]] bool IsBirthMessageType() const;

  void SendComplete(const MQTTAsync_successData& response);

  static void OnSendFailure(void *context, MQTTAsync_failureData *response);
  static void OnSend(void *context, MQTTAsync_successData *response);
  static void OnSubscribeFailure(void *context, MQTTAsync_failureData *response);
  static void OnSubscribe(void *context, MQTTAsync_successData *response);

};

} // pub_sub


