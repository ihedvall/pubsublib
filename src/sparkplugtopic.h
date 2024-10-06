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

 private:
  SparkplugNode& parent_;

  [[nodiscard]] bool IsValidMessageType() const;
  [[nodiscard]] bool IsBirthMessageType() const;

  void SendComplete(const MQTTAsync_successData& response);

  static void OnSendFailure(void *context, MQTTAsync_failureData *response);
  static void OnSendFailure5(void *context, MQTTAsync_failureData5 *response);


};

} // pub_sub


