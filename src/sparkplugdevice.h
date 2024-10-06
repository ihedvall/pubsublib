/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "pubsub/ipubsubclient.h"
#include <atomic>

namespace pub_sub {

class SparkplugNode;

class SparkplugDevice : public IPubSubClient {
 public:
  SparkplugDevice() = delete;
  explicit SparkplugDevice(SparkplugNode& parent);

  bool IsOnline() const override;
  bool IsOffline() const override;

  ITopic* AddMetric(const std::shared_ptr<Metric>& value) override;
  ITopic* CreateTopic() override;

  bool Start() override;
  bool Stop() override;
  [[nodiscard]] bool IsConnected() const override;

  void Poll();
  void SetAllMetricsInvalid();


 private:
  SparkplugNode& parent_;
  uint64_t sequence_number_ = 0; ///< Birth/Death sequence number

  enum class DeviceState {
    Idle,
    Offline,
    Online,
  };
  std::atomic<DeviceState> device_state_ = DeviceState::Idle;

  void CreateDeviceDeathTopic();
  void CreateDeviceBirthTopic();

  void PublishDeviceBirth();
  void PublishDeviceDeath();

};

} // pub_sub

