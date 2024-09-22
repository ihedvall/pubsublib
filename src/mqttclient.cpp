/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include "mqttclient.h"

#include <chrono>
#include <functional>

#include "util/logstream.h"
#include "util/utilfactory.h"
#include "util/timestamp.h"

#include "mqtttopic.h"

using namespace std::chrono_literals;
using namespace util::log;
using namespace util::time;

namespace {

} // end namespace

namespace pub_sub {
MqttClient::MqttClient()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))) {
  ResetConnectionLost();

}
MqttClient::~MqttClient() {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Stopping client");
  }
  MqttClient::Stop();
  for (auto& topic : topic_list_ ) {
    if (!topic) {
      continue;
    }
    auto& value = topic->Value();
    if (value) {
      value->SetPublish(nullptr);
    }
  }
  listen_.reset();
}

ITopic *MqttClient::CreateTopic() {

  auto topic = std::make_unique<MqttTopic>(*this);
  std::scoped_lock list_lock(topic_mutex);

  topic_list_.emplace_back(std::move(topic));
  return topic_list_.back().get();
}


ITopic *MqttClient::AddMetric(const std::shared_ptr<Metric>& value) {
  auto* topic = CreateTopic(); // Note that this call adds the topic to its list.
  if ( topic == nullptr) {
    return nullptr;
  }
  topic->Topic(value->Name());

  // Set default value
  const auto payload = value->GetMqttString();
  if (value->IsNull()) {
    topic->PayloadBody("");
  } else {
    topic->PayloadBody(payload);
  }
  value->SetPublish([this] (Metric& value) { OnPublish(value); });
  topic->Value(value);

  return topic;
}

bool MqttClient::IsConnected() const {
  return handle_ != nullptr && MQTTAsync_isConnected(handle_);
}

bool MqttClient::Start() {

  if (listen_ && !Name().empty()) {
    listen_->PreText(Name());
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
  if (listen_->IsActive() && listen_) {
    listen_->ListenText("Creating client");
  }

  const auto create = MQTTAsync_create(&handle_, connect_string.str().c_str(),
                                       name_.c_str(),
                                 MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (create != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to create the MQTT handle.";
    const auto* cause = MQTTAsync_strerror(create);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }

  const auto callback = MQTTAsync_setCallbacks(handle_, this,
                                               OnConnectionLost,
                                               OnMessageArrived,
                                               OnDeliveryComplete);
  if (callback != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to set the MQTT callbacks.";
    const auto* cause = MQTTAsync_strerror(callback);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }
  if (listen_->IsActive() && listen_) {
    listen_->ListenText("Started client");
  }
  return SendConnect();
}

bool MqttClient::SendConnect() {

  // Reset the connection lost to detect any failing startup
  ResetConnectionLost();

  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.keepAliveInterval = 60; // 60 seconds between keep alive messages
  connect_options.cleansession = MQTTASYNC_TRUE;
  connect_options.connectTimeout = 10; // Wait max 10 seconds on connect.
  connect_options.onSuccess = OnConnect;
  connect_options.onFailure = OnConnectFailure;
  connect_options.context = this;

  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to connect to the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(connect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }
  return true;
}

bool MqttClient::Stop() {
  if (!IsConnected()) {
    if (listen_ && listen_->IsActive() && listen_->LogLevel() == 3) {
      listen_->ListenText("Stop ignored due to not connected to server");
    }
    return true;
  }

  if (listen_ && listen_->IsActive() && listen_->LogLevel() == 3) {
    listen_->ListenText("Disconnecting");
  }
  ResetConnectionLost();
  MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
  disconnect_options.onSuccess = OnDisconnect;
  disconnect_options.onFailure = OnDisconnectFailure;
  disconnect_options.context = this;
  disconnect_options.timeout = 5000;
  disconnect_ready_ = false;
  const auto disconnect = MQTTAsync_disconnect(handle_, &disconnect_options);
  if (disconnect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to disconnect from the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(disconnect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("%s", err.str().c_str());
    }
  }
  // Add a delay before deleting the async context handle so the disconnect get through.
  // The timeout is set to 5 seconds so wait max 10 seconds for a reply.
  for (size_t timeout = 0;
       !disconnect_ready_ && disconnect == MQTTASYNC_SUCCESS && timeout < 1'000;
       ++timeout) {
    std::this_thread::sleep_for(10ms);
  }
  if (listen_ && listen_->IsActive() && listen_->LogLevel() == 3) {
    listen_->ListenText("Disconnected");
  }
  MQTTAsync_destroy(&handle_);
  handle_ = nullptr;
  return true;
}

void MqttClient::ConnectionLost(const std::string& cause) {
  std::ostringstream err;
  err << "Connection lost.";
  if (!cause.empty()) {
    err << " Error: " << cause;
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str() );
  }
  SetConnectionLost();
}

void MqttClient::Message(const std::string& topic_name, const MQTTAsync_message& message) {
  ResetConnectionLost();
  const auto qos = static_cast<QualityOfService>(message.qos);
  std::vector<uint8_t> payload(message.payloadlen, 0);
  if (message.payload != nullptr && message.payloadlen > 0) {
    std::memcpy(payload.data(), message.payload, message.payloadlen);
  }
  auto* topic = GetTopic(topic_name);
  if (topic == nullptr) {
    topic = CreateTopic();
    topic->Topic(topic_name);
  }

  topic->PayloadBody(payload);
  topic->Qos(qos);
  topic->Retained(message.retained == 1);
  const auto& value = topic->Value();
  if (value) {
    const auto now = TimeStampToNs(); // MQTT doesn't supply a network time so use the computer time.
    value->Timestamp(now);
    const auto text = topic->PayloadBody<std::string>();
    value->Value(text);
    value->OnUpdate();
  }

  if (listen_ && listen_->IsActive() && listen_->LogLevel() != 1) {
    listen_->ListenText("Sub-Topic: %s, Value: %s", topic_name.c_str(),
                                topic->PayloadBody<std::string>().c_str());
  }
}

void MqttClient::DeliveryComplete(MQTTAsync_token token) {
  ResetConnectionLost();
}


void MqttClient::Connect(const MQTTAsync_successData* response) {

  if (response != nullptr && listen_ && listen_->IsActive()) {
    const std::string server_url = response->alt.connect.serverURI;
    const int version = response->alt.connect.MQTTVersion;
    const int session_present = response->alt.connect.sessionPresent;
    listen_->ListenText("Connected: Server: %s, Version: %d, Session: %d",
                                server_url.c_str(), version, session_present);
  }
  ResetConnectionLost();
  DoConnect();
}


void MqttClient::ConnectFailure( MQTTAsync_failureData* response) {
  std::ostringstream err;
  err << "Connect failure.";
  if (response != nullptr) {
    const auto code = response->code;
    const auto* cause = MQTTAsync_strerror(code);
    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause;
    }

  }
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetConnectionLost();
}

void MqttClient::Disconnect(MQTTAsync_successData* response) {
  disconnect_ready_ = true;
  ResetConnectionLost();
}

void MqttClient::DisconnectFailure(MQTTAsync_failureData* response) {
  if (response != nullptr) {
    std::ostringstream err;
    err << "Disconnect failure.";
    const int code = response->code;
    const auto* cause = MQTTAsync_strerror(code);
    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause;
    }

    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("%s", err.str().c_str());
    }
    SetConnectionLost();
  }
  disconnect_ready_ = true;
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

void MqttClient::OnConnectionLost(void *context, char *cause) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  std::string reason = cause != nullptr ? cause : "";
  if (client != nullptr) {
    client->ConnectionLost(reason);
  }
  if (cause != nullptr) {
    MQTTAsync_free(cause);
  }
}

int MqttClient::OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  const std::string topic_id = topic_name != nullptr && topicLen > 0 ? topic_name : "";

  if (client != nullptr && message != nullptr && !topic_id.empty()) {
    client->Message(topic_id, *message);
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
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->DeliveryComplete(token);
  }
}

void MqttClient::OnConnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr && response != nullptr) {
    client->Connect(response);
  }
}

void MqttClient::OnConnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->ConnectFailure(response);
  }
}

void MqttClient::OnDisconnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->Disconnect(response);
  }
}

void MqttClient::OnDisconnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->DisconnectFailure(response);
  }
}

void MqttClient::OnPublish(Metric &value) {
  auto* topic = GetTopic(value.Name());
  if (topic == nullptr) {
    return;
  }
  const auto payload_string = value.GetMqttString();
  topic->PayloadBody(payload_string);
}

bool MqttClient::IsOnline() const {
  return IsConnected();
}
bool MqttClient::IsOffline() const {
  return !IsConnected();
}

} // end namespace