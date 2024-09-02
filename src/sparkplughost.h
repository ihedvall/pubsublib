/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once


#include "sparkplugnode.h"

#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <MQTTAsync.h>
#include <util/ilisten.h>
namespace pub_sub {

class SparkplugHost : public SparkplugNode {
 public:
  SparkplugHost();
  bool Start() override; ///< Starts the working thread.
  bool Stop() override;  ///< Stops the working thread.

  [[nodiscard]] bool IsOnline() const override;
  [[nodiscard]] bool IsOffline() const override;
 protected:
  bool SendConnect() override;
 private:

  std::thread work_thread_; ///< Handles the online connect and subscription
  ITopic* CreateStateTopic();
  uint64_t start_time_ = 0;

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

  uint64_t work_timer_ = 0; ///< Indicate not started
  std::atomic<bool> stop_work_task_ = true;
  std::condition_variable host_event_;
  std::mutex host_mutex_;

  void WorkerTask();
  void PublishState(bool online);





};

} // pub_sub

