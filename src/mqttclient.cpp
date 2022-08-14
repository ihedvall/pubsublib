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

namespace {

} // end namespace

namespace pub_sub {
MqttClient::MqttClient()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))) {

}
MqttClient::~MqttClient() {
  if (listen_->IsActive()) {
    listen_->ListenText("Stopping client");
  }
  MqttClient::Stop();
  listen_.reset();
}

void MqttClient::Start() {
  if (!ClientId().empty()) {
    listen_->PreText(ClientId());
  }
  std::ostringstream temp;
  temp << "tcp://" << Broker() << ":" << Port();
  connect_string_ = temp.str();
  if (listen_->IsActive()) {
    listen_->ListenText("Creating client");
  }

  const auto create = MQTTAsync_create(&handle_, connect_string_.c_str(), client_id_.c_str(),
                                 MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (create != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to create the MQTT handle. Error: " << MQTTAsync_strerror(create);
    throw std::runtime_error(err.str());
  }

  const auto callback = MQTTAsync_setCallbacks(handle_, this, OnConnectionLost, OnMessageArrived, OnDeliveryComplete);
  if (create != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to set the MQTT callbacks. Error: " << MQTTAsync_strerror(callback);
    throw std::runtime_error(err.str());
  }
  if (listen_->IsActive()) {
    listen_->ListenText("Start client");
  }

  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.keepAliveInterval = 60; // 60 seconds between keep alive messages
  connect_options.cleansession = MQTTASYNC_TRUE;
  connect_options.connectTimeout = 10; // Wait max 10 seconds on connect.
  connect_options.onSuccess = MqttClient::OnConnect;
  connect_options.onFailure = MqttClient::OnConnectFailure;
  connect_options.context = this;
  connected_ = false;
  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to connect to the MQTT broker. Error: " << MQTTAsync_strerror(connect);
    throw std::runtime_error(err.str());
  }
}

void MqttClient::Stop() {
  if (!MQTTAsync_isConnected(handle_)) {
    return;
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
  MQTTAsync_destroy(&handle_);
}

void MqttClient::OnConnectionLost(void *context, char *cause) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    if (client->listen_->IsActive()) {
      client->listen_->ListenText("Connection lost. Cause: %s", (cause == nullptr ? "Unknown" : cause) );
    }
    client->connected_ = false;
  }
  MQTTAsync_free(cause);
}

int MqttClient::OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message) {
  auto *client = reinterpret_cast<MqttClient *>(context);

  if ( client != nullptr && message != nullptr) {
    const auto qos = message->qos;
    std::string topic_id = topic_name != nullptr ? topic_name : "";
    std::vector<uint8_t> payload(message->payloadlen, 0);
    if (message->payload != nullptr && message->payloadlen > 0) {
      memcpy_s(payload.data(), payload.size(), message->payload, message->payloadlen);
    }
    if (client->wildcard_search_ ) {
      client->wildcard_list_.insert(topic_id);
      ++client->wildcard_count_;
      if (client->listen_->IsActive()) {
        client->listen_->ListenText("Topic: %s",topic_id.c_str());
      }
    } else {
      auto &topic_list = client->Topics();
      for (auto &topic: topic_list) {
        if (!topic) {
          continue;
        }

        if (topic->Topic() == topic_id) {
          topic->Payload(payload);
          topic->Qos(static_cast<QualityOfService>(qos));
          if (client->listen_->IsActive() && client->listen_->LogLevel() != 1) {
            client->listen_->ListenText("Sub-Topic: %s, Value: %s",
                                        topic_id.c_str(),
                                        topic->Payload<std::string>().c_str());
          }
        }
      }
    }
  }
  MQTTAsync_free(topic_name);
  MQTTAsync_freeMessage(&message);
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
    client->connected_ = true;
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
    client->connected_ = false;
  }
}

void MqttClient::OnDisconnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {

    if (client->listen_->IsActive()) {
      client->listen_->ListenText("Disconnected");
    }
    client->disconnect_ready_ = true;
    client->connected_ = false;
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

bool MqttClient::FetchTopics(const std::string &search_topic, std::set<std::string>& topic_list) {
  MqttClient client;
  client.Broker(Broker());
  client.Port(Port());
  client.wildcard_search_ = true;

  auto subscribe = client.CreateTopic();
  subscribe->Topic(search_topic);
  subscribe->Qos(QualityOfService::Qos1);
  subscribe->Publish(false);

  client.Start();
  uint64_t count = client.wildcard_count_;
  for (size_t index = 0; index < 10; ++index) {
    if (count != client.wildcard_count_) {
      index = 0;
      count = client.wildcard_count_;
    }
    std::this_thread::sleep_for(100ms);
  }
  client.Stop();
  topic_list = client.wildcard_list_;
  return true;
}

} // end namespace