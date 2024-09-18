/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once


#include "sparkplugnode.h"
#include <atomic>
#include <MQTTAsync.h>
#include <util/ilisten.h>
namespace pub_sub {

class SparkplugHost : public SparkplugNode {
 public:
  SparkplugHost();
  ~SparkplugHost() override;

  bool Start() override; ///< Starts the working thread.
  bool Stop() override;  ///< Stops the working thread.

  [[nodiscard]] bool IsOnline() const override;
  [[nodiscard]] bool IsOffline() const override;


 protected:
  bool SendConnect() override;
 private:

  enum class WorkState {
    Idle,             ///< Initial state, try to connect periodically
    WaitOnConnect,    ///< Wait on connect
    WaitOnline,       ///< Handle subscription values
    Online,
    WaitOffline,
    Offline,
    WaitOnDisconnect
  };
  std::atomic<WorkState> work_state_ = WorkState::Idle;
  std::atomic<bool> stop_work_task_ = true;

  ITopic* CreateStateTopic();

  [[nodiscard]] IPubSubClient* CreateDevice(const std::string& device_name) override;
  void DeleteDevice(const std::string& device_name) override;

  [[nodiscard]] IPubSubClient* GetDevice(const std::string& device_name) override;
  [[nodiscard]] const IPubSubClient* GetDevice(const std::string& device_name) const override;

  uint64_t start_time_ = 0;
  void HostTask();
  void PublishState(bool online);





};

} // pub_sub

