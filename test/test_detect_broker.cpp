/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

/** \file
 * Unit tests of the DetectBroker class
 */

#include <gtest/gtest.h>
#include "pubsub/pubsubfactory.h"

#include <string_view>
#include <array>

namespace {
  constexpr std::array<std::string_view, 3> kBrokerList = {
      "127.0.0.1", // Broker is on this machine
      "192.168.66.21", // Broker is on a NAS
      "test.mosquitto.org"
  };
  constexpr uint16_t kDefaultPort = 1883;

}
namespace pub_sub::test {

TEST(TestDetectBroker, DetectBrokerVersion3) {
  auto detect = PubSubFactory::CreatePubSubClient(PubSubType::DetectMqttBroker);
  ASSERT_TRUE(detect);
  bool found = false;
  std::string name;
  std::string broker_name;
  std::string version;
  std::cout << "DETECT VERSION 3" << std::endl;
  for (const auto& broker : kBrokerList) {
    detect->Broker(broker.data());
    detect->Transport(TransportLayer::MqttTcp);
    detect->Port(kDefaultPort);
    found = detect->Start();
    std::cout << detect->Broker() << ": " << (found ? "Found" : "Not Found") << std::endl;
    detect->Stop();
    if (found) {
      name = detect->Name();
      broker_name = detect->Broker();
      version = detect->VersionAsString();
      std::cout << "Name: " << name << std::endl;
      std::cout << "Broker: " << broker_name << std::endl;
      std::cout << "Version: " << version << std::endl;
    }
    std::cout << std::endl;
  }
}

TEST(TestDetectBroker, DetectBrokerVersion5) {
  auto detect = PubSubFactory::CreatePubSubClient(PubSubType::DetectMqttBroker);
  ASSERT_TRUE(detect);
  bool found = false;
  std::string name;
  std::string broker_name;
  std::string version;
  std::cout << "DETECT VERSION 5" << std::endl;
  for (const auto& broker : kBrokerList) {
    detect->Broker(broker.data());
    detect->Transport(TransportLayer::MqttTcp);
    detect->Port(kDefaultPort);
    detect->Version(ProtocolVersion::Mqtt5);
    found = detect->Start();
    detect->Stop();
    std::cout << detect->Broker() << ": " << (found ? "Found" : "Not Found") << std::endl;
    if (found) {
      name = detect->Name();
      broker_name = detect->Broker();
      version = detect->VersionAsString();
      std::cout << "Name: " << name << std::endl;
      std::cout << "Broker: " << broker_name << std::endl;
      std::cout << "Version: " << version << std::endl;
    }
    std::cout << std::endl;
  }
}

} // End namespace pub_sub::test
