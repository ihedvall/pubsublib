/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <atomic>
#include <MQTTAsync.h>
#include <util/ilisten.h>
#include "mqttclient.h"

namespace pub_sub {

class SparkPlugNode : public MqttClient {
 public:
  SparkPlugNode();
  ~SparkPlugNode() override;
  bool Start() override;
  bool Stop() override;
 protected:
  bool SendConnect();
 private:
  uint64_t birth_death_sequence_number = 0;
  ITopic* CreateNodeDeathTopic();
  ITopic* CreateNodeBirthTopic();
};

} // pub_sub
