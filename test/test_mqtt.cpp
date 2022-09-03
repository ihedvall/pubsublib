/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include <string>
#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include <MQTTAsync.h>
#include <util/utilfactory.h>
#include <util/logconfig.h>
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

namespace {

constexpr std::string_view kMqttBroker = "tcp://192.168.1.155:1883";
constexpr std::string_view kClientId = "ExampleClientPub";
constexpr std::string_view kTopic = "test/Hello";
std::string kPayLoad = "Hello Ingemar Hedvall";
constexpr int kQos = 1;

int finished = 0;

void OnConnectionLost(void *context, char *cause) {
  auto client = reinterpret_cast<MQTTAsync> (context);
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  int rc = 0;

  printf("\nConnection lost\n");
  printf("     cause: %s\n", cause);

  printf("Reconnecting\n");
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;
  if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to start connect, return code %d\n", rc);
    finished = 1;
  }
}

void OnDisconnectFailure(void *context, MQTTAsync_failureData *response) {
  printf("Disconnect failed\n");
  finished = 1;
}

void OnDisconnect(void *context, MQTTAsync_successData *response) {
  printf("Successful disconnection\n");
  finished = 1;
}

void OnSendFailure(void *context, MQTTAsync_failureData *response) {
  MQTTAsync client = (MQTTAsync) context;
  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  int rc;

  printf("Message send failed token %d error code %d\n", response->token, response->code);
  opts.onSuccess = OnDisconnect;
  opts.onFailure = OnDisconnectFailure;
  opts.context = client;
  if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to start disconnect, return code %d\n", rc);
    exit(EXIT_FAILURE);
  }
}

void OnSend(void *context, MQTTAsync_successData *response) {
  auto client = reinterpret_cast<MQTTAsync>(context);
  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  int rc;

  printf("Message with token value %d delivery confirmed\n", response->token);
  opts.onSuccess = OnDisconnect;
  opts.onFailure = OnDisconnectFailure;
  opts.context = client;
  if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to start disconnect, return code %d\n", rc);
    exit(EXIT_FAILURE);
  }
}

void OnConnectFailure(void *context, MQTTAsync_failureData *response) {
  printf("Connect failed, rc %d\n", response ? response->code : 0);
  finished = 1;
}

void OnConnect(void *context, MQTTAsync_successData *response) {
  auto client = reinterpret_cast<MQTTAsync>(context);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
  int rc;

  printf("Successful connection\n");
  opts.onSuccess = OnSend;
  opts.onFailure = OnSendFailure;
  opts.context = client;

  pubmsg.payload = kPayLoad.data();
  pubmsg.payloadlen = static_cast<int>(kPayLoad.size());
  pubmsg.qos = kQos;
  pubmsg.retained = 1;
  if ((rc = MQTTAsync_sendMessage(client, kTopic.data(), &pubmsg, &opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to start sendMessage, return code %d\n", rc);
    exit(EXIT_FAILURE);
  }
}

int MessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *m) {
  // not expecting any messages
  return 1;
}

} // end namespace

namespace pub_sub::test {
TEST(Mqtt, DISABLED_PublishV3) { // NOLINT
  MQTTAsync client = nullptr;
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  int rc = 0;

  if ((rc = MQTTAsync_create(&client, kMqttBroker.data(), kClientId.data(), MQTTCLIENT_PERSISTENCE_NONE, nullptr)) != MQTTASYNC_SUCCESS) {
    printf("Failed to create client object, return code %d\n", rc);
    exit(EXIT_FAILURE);
  }

  if ((rc = MQTTAsync_setCallbacks(client, nullptr, OnConnectionLost, MessageArrived, nullptr)) != MQTTASYNC_SUCCESS) {
    printf("Failed to set callback, return code %d\n", rc);
    exit(EXIT_FAILURE);
  }

  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;
  conn_opts.onSuccess = OnConnect;
  conn_opts.onFailure = OnConnectFailure;
  conn_opts.context = client;
  if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to start connect, return code %d:%s\n", rc, MQTTAsync_strerror(rc));
    exit(EXIT_FAILURE);
  }

  printf("Waiting for publication of %s\n"
         "on topic %s for client with ClientID: %s\n",
         kPayLoad.data(), kTopic.data(), kClientId.data());
  while (!finished) {
    std::this_thread::sleep_for(100ms);
  }

  MQTTAsync_destroy(&client);
}

TEST(Mqtt, Mqtt3Client) { // NOLINT
  auto& log_config = LogConfig::Instance();
  log_config.Type(LogType::LogToConsole);
  log_config.CreateDefaultLogger();

  auto listen = util::UtilFactory::CreateListen("ListenConsole", "LISMQTT");

  listen->Start();
  listen->SetActive(true);
  listen->SetLogLevel(3);
  std::this_thread::sleep_for(1s);

  auto client = PubSubFactory::CreatePubSubClient(PubSubType::Mqtt3Client);
  client->Broker("test.mosquitto.org");
  client->Port(1883);
  client->ClientId("Client1");

  auto publish = client->CreateTopic();
  publish->Topic("ihedvall/test/pubsub/test_mqtt3");
  publish->Payload(kPayLoad.data());
  publish->Qos(QualityOfService::Qos1);
  publish->Retained(true);
  publish->Publish(true);

  auto subscribe = client->CreateTopic();
  subscribe->Topic("ihedvall/test/pubsub/test_mqtt3");
  subscribe->Qos(QualityOfService::Qos1);
  subscribe->Publish(false);

  client->Start();

  std::this_thread::sleep_for(1s);

  for (size_t index = 0; index < 10; ++index) {
    std::ostringstream temp;
    temp << "Pelle_" << index;
    publish->Payload(temp.str());
    std::this_thread::sleep_for(1s);
  }

  client->Stop();
  listen->Stop();

  client.reset();
  listen.reset();
  log_config.DeleteLogChain();
}

} // end namespace