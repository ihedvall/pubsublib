/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "detectbroker.h"
#include <chrono>

using namespace std::chrono_literals;
namespace pub_sub {
bool DetectBroker::Start() {
  const auto start = MqttClient::Start();
  if (!start) {
    return false;
  }
  bool connected = false;
  for (size_t delay = 0; delay < 1'000; ++delay) {
    if (IsConnectionLost()) {
      connected = false;
      break;
    }
    if (IsConnected()) {
      connected = true;
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  return connected;
}

} // pub_sub