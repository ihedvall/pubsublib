/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "test_sparkplug.h"
#include <chrono>

#include <MQTTAsync.h>
#include <util/utilfactory.h>
#include <util/logconfig.h>
#include <util/logstream.h>
#include <util/timestamp.h>
#include "pubsub/pubsubfactory.h"

using namespace util::log;
using namespace std::chrono_literals;

namespace {
  constexpr std::string_view kLocalBroker = "127.0.0.1";
  constexpr std::string_view kLanBroker = "192.168.66.21";
  constexpr std::string_view kMosquittoBroker = "test.mosquitto.org";
  constexpr std::string_view kHost = "Host1";
  constexpr std::string_view kGroup = "Group1";
  constexpr std::string_view kNode = "Node1";
  constexpr uint16_t kBasicPort = 1883; ///< Simple no TLS port
  constexpr uint16_t kFailingPort = 1773; ///< Dummy port testing

  std::string kBroker; ///< Broker in use
  std::string kBrokerName; ///< Name of broker in use
  std::unique_ptr<util::log::IListen> kListen; ///< Listen object
}

namespace pub_sub::test {
void TestSparkplug::SetUpTestSuite() {
   // Set up the logging to console
  auto& log_config = LogConfig::Instance();
  log_config.Type(LogType::LogToConsole);
  log_config.CreateDefaultLogger();
  auto* logger = log_config.GetLogger("Default");
  if (logger != nullptr) {
    logger->ShowLocation(false);
  }

    // Set up the listen to console
  kListen = util::UtilFactory::CreateListen("ListenConsole", "LISMQTT");
  kListen->Start();
  kListen->SetActive(true);
  kListen->SetLogLevel(3);

    // Search for a broker that exist.
  auto detect = PubSubFactory::CreatePubSubClient(PubSubType::DetectMqttBroker);
  detect->Broker(kLocalBroker.data());
  detect->Port(1883);
  detect->Name("LocalBroker");
  auto exist = detect->Start();
  detect->Stop();
  if (exist) {
    kBroker = detect->Broker();
    kBrokerName = detect->Name();
    return;
  }

  detect->Broker(kLanBroker.data());
  detect->Name("LanBroker");
  exist = detect->Start();
  detect->Stop();
  if (exist) {
    kBroker = detect->Broker();
    kBrokerName = detect->Name();
    return;
  }

  detect->Broker(kMosquittoBroker.data());
  detect->Name("MosquittoBroker");
  exist = detect->Start();
  detect->Stop();
  if (exist) {
    kBroker = detect->Broker();
    kBrokerName = detect->Name();
    return;
  }
}

void TestSparkplug::TearDownTestSuite() {
  kListen->Stop();
  kListen.reset();

  auto& log_config = LogConfig::Instance();
  log_config.DeleteLogChain();
}

TEST_F(TestSparkplug, TestPrimaryHost) {
  if (kBroker.empty()) {
    GTEST_SKIP_("No MQTT broker detected");
  }
  auto host = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugHost);
  ASSERT_TRUE(host);
  host->Broker(kBroker);
  host->Port(kBasicPort);
  host->Name(kHost.data());
  host->InService(false);

  const bool start = host->Start();
  ASSERT_TRUE(start);

  // Check that both clients are connected
  for (size_t connect = 0; connect < 1000; ++connect) {
    if (host->IsConnected() && host->IsOffline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_TRUE(host->IsConnected());
  EXPECT_TRUE(host->IsOffline());
  // Take In-Service
  host->InService(true);
  for (size_t online = 0; online < 1000; ++online) {
    if (host->IsOnline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_TRUE(host->IsOnline());

  // Take Off-Service
  host->InService(false);
  for (size_t offline = 0; offline < 1000; ++offline) {
    if (host->IsOffline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_TRUE(host->IsOffline());

  host->Stop();

  // Check that both clients are connected
  for (size_t disconnect = 0; disconnect < 1000; ++disconnect) {
    if (!host->IsConnected() ) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_FALSE(host->IsConnected());
  host.reset();
}

TEST_F(TestSparkplug, TestFailingHost) {
  if (kBroker.empty()) {
    GTEST_SKIP_("No MQTT broker detected");
  }
  auto host = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugHost);
  ASSERT_TRUE(host);
  host->Broker(kBroker);
  host->Port(kFailingPort); // Note: invalid port
  host->Name(kHost.data());
  host->InService(false);
  host->AddSubscription("spBv1.0/#");

  const bool start = host->Start();
  ASSERT_TRUE(start); // Should start but never connect

  // Check that it is not connected
  for (size_t connect = 0; connect < 200; ++connect) {
    if (host->IsConnected() || host->IsOffline() || host->IsOnline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_FALSE(host->IsConnected());
  EXPECT_FALSE(host->IsOffline());
  EXPECT_FALSE(host->IsOnline());

  EXPECT_TRUE(host->Stop());

  host.reset();
}

TEST_F(TestSparkplug, TestNode) {
  if (kBroker.empty()) {
    GTEST_SKIP_("No MQTT broker detected");
  }

  auto node = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugNode);
  ASSERT_TRUE(node);
  node->Broker(kBroker);
  node->Port(kBasicPort);
  node->Name(kNode.data());
  node->GroupId(kGroup.data());
  node->InService(false);

  auto* device1 = node->CreateDevice("Device1");
  ASSERT_TRUE(device1 != nullptr);
  device1->InService(true);

  std::shared_ptr<Metric> metric1 = std::make_shared<Metric>();
  metric1->Name("Metric1");
  metric1->Type(MetricType::Float);
  metric1->Unit("V");
  metric1->Value(5.33F);
  device1->AddMetric(metric1);


  auto* device2 = node->CreateDevice("Device2");
  ASSERT_TRUE(device2 != nullptr);
  device2->InService(true);


  const bool start = node->Start(); // Only starts a thread
  ASSERT_TRUE(start);

  const bool start1 = device1->Start();
  ASSERT_TRUE(start1);

  const bool start2 = device2->Start();
  ASSERT_TRUE(start2);

  EXPECT_FALSE(node->IsConnected());
  EXPECT_TRUE(node->IsOffline());
  node->InService(true);

  // Check that the node is connected and is ONLINE
  for (size_t connect = 0; connect < 1000; ++connect) {
    if (node->IsConnected() && node->IsOnline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }

  EXPECT_TRUE(node->IsConnected());
  EXPECT_TRUE(node->IsOnline());

  // Check that the devices are connected and is ONLINE
  for (size_t connect = 0; connect < 1000; ++connect) {
    if (device1->IsOnline() && device2->IsOnline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }

  EXPECT_TRUE(device1->IsOnline());
  EXPECT_TRUE(device2->IsOnline());

  // Todo: Update some metrics. Make a sleep for now.
  std::this_thread::sleep_for(1s);

  // Take Off-Service
  device2->InService(false);
  device1->InService(false);
  node->InService(false);
  for (size_t offline = 0; offline < 1000; ++offline) {
    if (node->IsOffline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_TRUE(node->IsOffline());

  EXPECT_TRUE(device2->Stop());
  EXPECT_TRUE(device1->Stop());
  EXPECT_TRUE(node->Stop());
  EXPECT_FALSE(node->IsConnected());
  node.reset();
}

} // pub_sub::test