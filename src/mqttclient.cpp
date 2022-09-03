/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include <chrono>
#include "util/logstream.h"
#include "util/utilfactory.h"
#include "mqttclient.h"
#include "mqtttopic.h"

using namespace std::chrono_literals;
using namespace util::log;

namespace {

} // end namespace

namespace pub_sub {
MqttClient::MqttClient()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))) {

}
MqttClient::~MqttClient() {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Stopping client");
  }
  MqttClient::Stop();
  listen_.reset();
}

bool MqttClient::IsConnected() const {
  return handle_ != nullptr && MQTTAsync_isConnected(handle_);
}

bool MqttClient::Start() {
  if (!ClientId().empty()) {
    listen_->PreText(ClientId());
  }

  std::ostringstream connect_string;
  switch (Transport()) {
    case TransportLayer::MqttWebSocket:
      connect_string << "ws://";
      break;

    case TransportLayer::MqttTcpTls:
      connect_string << "ssl://";
      break;

    case TransportLayer::MqttWebSocketTls:
      connect_string << "wss://";
      break;

    default:
      connect_string << "tcp://";
      break;
  }
  connect_string << Broker() << ":" << Port();
  if (listen_->IsActive()) {
    listen_->ListenText("Creating client");
  }

  const auto create = MQTTAsync_create(&handle_, connect_string.str().c_str(), client_id_.c_str(),
                                 MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (create != MQTTASYNC_SUCCESS) {
    LOG_ERROR() << "Failed to create the MQTT handle. Error: " << MQTTAsync_strerror(create);
    return false;
  }

  const auto callback = MQTTAsync_setCallbacks(handle_, this, OnConnectionLost, OnMessageArrived, OnDeliveryComplete);
  if (create != MQTTASYNC_SUCCESS) {
    LOG_ERROR() << "Failed to set the MQTT callbacks. Error: " << MQTTAsync_strerror(callback);
    return false;
  }
  if (listen_->IsActive()) {
    listen_->ListenText("Start client");
  }
  return SendConnect();
}

bool MqttClient::SendConnect() {
  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.keepAliveInterval = 60; // 60 seconds between keep alive messages
  connect_options.cleansession = MQTTASYNC_TRUE;
  connect_options.connectTimeout = 10; // Wait max 10 seconds on connect.
  connect_options.onSuccess = MqttClient::OnConnect;
  connect_options.onFailure = MqttClient::OnConnectFailure;
  connect_options.context = this;



  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    LOG_ERROR()  << "Failed to connect to the MQTT broker. Error: " << MQTTAsync_strerror(connect);
    return false;
  }
  return true;
}

bool MqttClient::Stop() {
  if (handle_ == nullptr) {
    if (listen_->IsActive() && listen_->LogLevel() == 3) {
      listen_->ListenText("Stop ignored due to not connected to server");
    }
    return true;
  }

  if (listen_->IsActive() && listen_->LogLevel() == 3) {
    listen_->ListenText("Disconnecting");
  }
  MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
  disconnect_options.onSuccess = MqttClient::OnDisconnect;
  disconnect_options.onFailure = MqttClient::OnDisconnectFailure;
  disconnect_options.context = this;
  disconnect_options.timeout = 5000;
  disconnect_ready_ = false;
  const auto disconnect = MQTTAsync_disconnect(handle_, &disconnect_options);
  if (disconnect != MQTTASYNC_SUCCESS && listen_->IsActive()) {
    listen_->ListenText("Failed to disconnect from the MQTT broker. Error: %s", MQTTAsync_strerror(disconnect));
  }
  // Add a delay before deleting the async context handle so the disconnect get through.
  // The timeout is set to 5 seconds so wait max 10 seconds for a reply.
  for (size_t timeout = 0; !disconnect_ready_ && disconnect == MQTTASYNC_SUCCESS && timeout < 100; ++timeout) {
    std::this_thread::sleep_for(100ms);
  }
  if (listen_->IsActive() && listen_->LogLevel() == 3) {
    listen_->ListenText("Disconnected");
  }
  MQTTAsync_destroy(&handle_);
  handle_ = nullptr;
  return true;
}

void MqttClient::OnConnectionLost(void *context, char *cause) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    if (client->listen_->IsActive()) {
      client->listen_->ListenText("Connection lost. Cause: %s", (cause == nullptr ? "Unknown" : cause) );
    }
  }
  MQTTAsync_free(cause);
}
void MqttClient::OnMessage(const std::string& topic_name, MQTTAsync_message& message) {
  const auto qos = static_cast<QualityOfService>(message.qos);
  std::vector<uint8_t> payload(message.payloadlen, 0);
  if (message.payload != nullptr && message.payloadlen > 0) {
    memcpy_s(payload.data(), payload.size(), message.payload, message.payloadlen);
  }
  auto* topic = GetTopic(topic_name);
  if (topic == nullptr) {
    topic = CreateTopic();
    topic->Topic(topic_name);
  }

  topic->Payload(payload);
  topic->Qos(qos);
  topic->Retained(message.retained == 1);

  if (listen_ && listen_->IsActive() && listen_->LogLevel() != 1) {
    listen_->ListenText("Sub-Topic: %s, Value: %s", topic_name.c_str(),
                                topic->Payload<std::string>().c_str());
  }

}
int MqttClient::OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  const std::string topic_id = topic_name != nullptr && topicLen == 0 ? topic_name : "";

  if ( client != nullptr && message != nullptr && !topic_id.empty()) {
    client->OnMessage(topic_id, *message);
  }

  if (topic_name != nullptr) {
    MQTTAsync_free(topic_name);
  }
  if (message != nullptr) {
    MQTTAsync_freeMessage(&message);
  }
  return MQTTASYNC_TRUE;
}

void MqttClient::OnDeliveryComplete(void *context, MQTTAsync_token token) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    if (client->listen_->IsActive() && client->listen_->LogLevel() == 3) {
      client->listen_->ListenText("Delivered: Token: %d", token);
    }
  }
}

void MqttClient::OnConnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    if (response != nullptr && client->listen_->IsActive()) {
      const std::string server_url = response->alt.connect.serverURI;
      const int version = response->alt.connect.MQTTVersion;
      const int session_present = response->alt.connect.sessionPresent;
      client->listen_->ListenText("Connected: Server: %s, Version: %d, Session: %d",
                                  server_url.c_str(), version, session_present);
    }
    client->DoConnect();
  }
}

void MqttClient::OnConnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    if (response != nullptr && client->listen_->IsActive()) {
      const int code = response->code;
      client->listen_->ListenText("Connect failure. Code: %s", MQTTAsync_strerror(code));
    }
  }
}

void MqttClient::OnDisconnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {

    if (client->listen_->IsActive()) {
      client->listen_->ListenText("Disconnected callback");
    }
    client->disconnect_ready_ = true;
  }
}

void MqttClient::OnDisconnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    if (response != nullptr && client->listen_->IsActive()) {
      const int code = response->code;
      client->listen_->ListenText("Disconnection failure. Code: %s", MQTTAsync_strerror(code));
    }
    client->disconnect_ready_ = true;
  }
}

ITopic *MqttClient::CreateTopic() {

  auto topic = std::make_unique<MqttTopic>(*this);
  std::scoped_lock list_lock(topic_mutex);

  topic_list_.emplace_back(std::move(topic));
  return topic_list_.back().get();
}

void MqttClient::DoConnect() {
  for (auto& topic : topic_list_) {
    if (!topic) {
      continue;
    }
    if (topic->Publish() && topic->Updated()) {
      topic->DoPublish();
    } else if (!topic->Publish()) {
      topic->DoSubscribe();
    }
  }
}



} // end namespace