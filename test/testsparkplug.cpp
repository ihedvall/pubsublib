/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "testsparkplug.h"
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
  // Delay some time so the client have some time to connect
  // otherwise we miss the startup messages.
  std::this_thread::sleep_for(2s);

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
  host->Port(1883);
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
    if (!host->IsOnline()) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_FALSE(host->IsOnline());

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

} // pub_sub::test