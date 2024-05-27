/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include "test_mqtt.h"
#include <string>
#include <thread>
#include <chrono>

#include <MQTTAsync.h>
#include <util/utilfactory.h>
#include <util/logconfig.h>
#include <util/logstream.h>
#include <util/timestamp.h>
#include "pubsub/pubsubfactory.h"


/*
#if !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif

#if defined(_WRS_KERNEL)
#include <OsWrapper.h>
#endif
*/

using namespace std::chrono_literals;
using namespace util::log;
using namespace util::time;
namespace {
constexpr std::string_view kLocalBroker = "127.0.0.1";
constexpr std::string_view kLanBroker = "192.168.66.21";
constexpr std::string_view kMosquittoBroker = "test.mosquitto.org";
constexpr std::string_view kClientId = "ExampleClientPub";
constexpr std::string_view kTopic = "test/Hello";
std::string kPayLoad = "Hello Ingemar Hedvall";

class Shared {
 public:
  Shared() {
    LOG_DEBUG() << "Create shared object.";
  }

  ~Shared() {
    LOG_DEBUG() << "Delete shared object.";
  }

  void Dummy() {

  }

  void CopySharedPtr(const std::shared_ptr<Shared>& shared_ptr) {
    copy_ptr_ = shared_ptr;
    LOG_DEBUG() << "Shared use count " << shared_ptr.use_count();
    LOG_DEBUG() << "Copy use count " << copy_ptr_.use_count();
    copy_ptr_->Dummy();
    copy_ptr_.reset();
  }

 private:
  // Note: Using a shared ptr that own itself object in this way
  // is a bad idea as the destructor is not run unless the above
  // pointer is reset manually.
  std::shared_ptr<Shared> copy_ptr_;
};

} // end namespace

namespace pub_sub::test {

std::string TestMqtt::broker_;
std::string TestMqtt::broker_name_;
std::unique_ptr<util::log::IListen> TestMqtt::listen_;

void TestMqtt::SetUpTestSuite() {
  auto& log_config = LogConfig::Instance();
  log_config.Type(LogType::LogToConsole);
  log_config.CreateDefaultLogger();
  auto* logger = log_config.GetLogger("Default");
  if (logger != nullptr) {
    logger->ShowLocation(false);
  }

  listen_ = util::UtilFactory::CreateListen("ListenConsole", "LISMQTT");

  listen_->Start();
  listen_->SetActive(true);
  listen_->SetLogLevel(3);
  std::this_thread::sleep_for(200ms);

  auto detect = PubSubFactory::CreatePubSubClient(PubSubType::DetectMqttBroker);
  detect->Broker(kLocalBroker.data());
  detect->Port(1883);
  detect->Name("LocalBroker");
  auto exist = detect->Start();
  detect->Stop();
  if (exist) {
    broker_ = detect->Broker();
    broker_name_ = detect->Name();
    return;
  }

  detect->Broker(kLanBroker.data());
  detect->Name("LanBroker");
  exist = detect->Start();
  detect->Stop();
  if (exist) {
    broker_ = detect->Broker();
    broker_name_ = detect->Name();
    return;
  }

  detect->Broker(kMosquittoBroker.data());
  detect->Name("MosquittoBroker");
  exist = detect->Start();
  detect->Stop();
  if (exist) {
    broker_ = detect->Broker();
    broker_name_ = detect->Name();
    return;
  }

}

void TestMqtt::TearDownTestSuite() {
  listen_->Stop();
  listen_.reset();

  auto& log_config = LogConfig::Instance();
  log_config.DeleteLogChain();
}

TEST_F(TestMqtt, SharedPtr) {
  auto orig_ptr = std::make_shared<Shared>();
  ASSERT_TRUE(orig_ptr);
  LOG_DEBUG() << "Orig use count " << orig_ptr.use_count();
  orig_ptr->CopySharedPtr(orig_ptr);

  LOG_DEBUG() << "Orig use count " << orig_ptr.use_count();
  orig_ptr.reset();
  LOG_DEBUG() << "Orig use count " << orig_ptr.use_count();
}

TEST_F(TestMqtt, IValue) {
  constexpr std::string_view value_name = "Value1";
  auto value = PubSubFactory::CreateValue(value_name);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->Name(), value_name);

  constexpr std::string_view unit = "km/h";
  value->Unit(unit.data());
  EXPECT_EQ(value->Unit(), unit);

  constexpr uint64_t alias = 1056;
  value->Alias(alias);
  EXPECT_EQ(value->Alias(), alias);

  const auto now = TimeStampToNs();
  value->Timestamp(now);
  EXPECT_EQ(value->Timestamp(), now);

  const auto type = ValueType::Boolean;
  value->Type(type);
  EXPECT_EQ(value->Type(), type);

  EXPECT_FALSE(value->IsHistorical());
  value->IsHistorical(true);
  EXPECT_TRUE(value->IsHistorical());
  value->IsHistorical(false);
  EXPECT_FALSE(value->IsHistorical());

  EXPECT_TRUE(value->IsNull()); // Default shall be true i.e. invalid
  value->IsNull(true);
  EXPECT_TRUE(value->IsNull());
  value->IsNull(false);
  EXPECT_FALSE(value->IsNull());

  value->Value(true);
  EXPECT_TRUE(value->Value<bool>());
  EXPECT_EQ(value->Value<int>(),1);
  EXPECT_EQ(value->Value<std::string>(),"1");

  value->Value(false);
  EXPECT_FALSE(value->Value<bool>());
  EXPECT_EQ(value->Value<int>(),0);
  EXPECT_EQ(value->Value<std::string>(),"0");

  auto unit_value = PubSubFactory::CreateValue(value_name);
  unit_value->Type(ValueType::Double);
  EXPECT_EQ(unit_value->Type(),ValueType::Double);
  EXPECT_TRUE(unit_value->Unit().empty());
  std::string sim_value = "100.1 ms";
  unit_value->Value(sim_value);
  EXPECT_TRUE(unit_value->Unit() == "ms");
  EXPECT_DOUBLE_EQ(unit_value->Value<double>(), 100.1);

}

TEST_F(TestMqtt, Mqtt3Client) { // NOLINT
  if (broker_.empty()) {
    GTEST_SKIP();
  }

  auto publisher = PubSubFactory::CreatePubSubClient(PubSubType::Mqtt3Client);
  publisher->Broker(broker_);
  publisher->Port(1883);
  publisher->Name("Pub");

  constexpr std::string_view string_name = "ihedvall/test/pubsub/string_value";
  auto write_value = PubSubFactory::CreateValue(string_name);
  write_value->Type(ValueType::String);
  write_value->Value("StringVal"); // Initial value

  auto publish = publisher->AddValue(write_value);

  publish->Payload(kPayLoad.data());
  publish->Qos(QualityOfService::Qos1);
  publish->Retained(true);
  publish->Publish(true);

  EXPECT_FALSE(publisher->IsConnected());
  publisher->Start();

  auto subscriber = PubSubFactory::CreatePubSubClient(PubSubType::Mqtt3Client);
  subscriber->Broker(broker_);
  subscriber->Port(1883);
  subscriber->Name("Sub");

  bool value_read = false;
  auto read_value = PubSubFactory::CreateValue(string_name);
  read_value->Type(ValueType::String);
  read_value->SetOnUpdate([&] () -> void {
    LOG_DEBUG() << "Value: " << read_value->Value<std::string>();
    value_read = true;
  });

  auto subscribe = subscriber->AddValue(read_value);
  subscribe->Qos(QualityOfService::Qos1);
  subscribe->Publish(false);
  EXPECT_FALSE(subscriber->IsConnected());

  subscriber->Start();

  // Check that both clients are connected
  for (size_t connect = 0; connect < 1000; ++connect) {
    if (publisher->IsConnected() && subscriber->IsConnected()) {
      break;
    }
    std::this_thread::sleep_for(1ms);
  }


  // Publish some dummy values
  for (size_t index = 0; index < 10; ++index) {
    value_read = false;
    std::ostringstream temp;
    temp << "Pelle_" << index;
    write_value->Value(temp.str());
    write_value->Publish();

    for (size_t timeout = 0; timeout < 10000; ++timeout) {
      if (value_read) {
        break;
      }
      std::this_thread::sleep_for(1ms);
    }
    if (!value_read) {
      FAIL() << "No value read";
      break;
    }

  }

  publisher->Stop();
  subscriber->Stop();
  // Check that both clients are connected
  for (size_t disconnect = 0; disconnect < 1000; ++disconnect) {
    if (!publisher->IsConnected() && !subscriber->IsConnected()) {
      break;
    }
    std::this_thread::sleep_for(1ms);
  }
  EXPECT_FALSE(publisher->IsConnected());
  EXPECT_FALSE(publisher->IsFaulty());
  EXPECT_FALSE(subscriber->IsConnected());
  EXPECT_FALSE(subscriber->IsFaulty());


}


} // end namespace