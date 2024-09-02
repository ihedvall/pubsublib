/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "sparkplughost.h"

#include <string_view>
#include <chrono>
#include "pubsub/itopic.h"
#include "util/logstream.h"
#include "sparkplughelper.h"
#include <boost/json.hpp>

using namespace util::log;
using namespace std::chrono_literals;

namespace {
  constexpr std::string_view kSparkplugNamespace = "spBv1.0";

}

namespace pub_sub {
SparkplugHost::SparkplugHost()
: SparkplugNode() {

}

bool SparkplugHost::Start() {
  SetFaulty(false, {});

  // Set the host ID as Listen pre-text debugger text
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
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Creating Host");
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
    SetFaulty(true, err.str());
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
    SetFaulty(true, err.str());
    LOG_ERROR() << err.str();
    return false;
  }
  if (listen_->IsActive() && listen_) {
    listen_->ListenText("Started client");
  }

  // Create the worker task
  if (work_thread_.joinable() ) {
    work_thread_.join();
  }
  start_time_ = SparkplugHelper::NowMs();
  CreateStateTopic();

  stop_work_task_ = false;
  work_thread_ = std::thread(&SparkplugHost::WorkerTask, this);
  return true;
}

bool SparkplugHost::Stop() {
  stop_work_task_ = true;
  host_event_.notify_one();
  if (work_thread_.joinable()) {
    work_thread_.join();
  }
  MQTTAsync_destroy(&handle_);
  handle_ = nullptr;
  return true;
}

ITopic* SparkplugHost::CreateStateTopic() {
  // Check if the STATE topic already exists
  auto* topic = GetTopicByMessageType("STATE");
  if (topic != nullptr) {
    return topic;
  }

  // Create the state topic
  std::ostringstream topic_name;
  topic_name << kSparkplugNamespace <<  "/STATE/" << Name();

  topic = CreateTopic();
  topic->Topic(topic_name.str());
  topic->Namespace(kSparkplugNamespace.data());
  topic->GroupId("");
  topic->MessageType("STATE");
  topic->NodeId(Name());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos1);
  topic->Retained(true);
  topic->ContentType("application/json");

  auto* online = topic->CreateMetric("online");
  online->Type(ValueType::Boolean);
  online->Value(false); // Meaning OFFLINE by default

  auto* timestamp = topic->CreateMetric("timestamp");
  timestamp->Type(ValueType::UInt64);
  timestamp->Value(start_time_);

  auto& payload = topic->GetPayload();
  payload.GenerateJson();
  return topic;
}

void SparkplugHost::WorkerTask() {
  // First send a connect message to the MQTT broker
  auto now = SparkplugHelper::NowMs();
  uint64_t timer = now; // This ends the Idle wait
  work_state_ = WorkState::Idle;

  while (!stop_work_task_) {
    std::unique_lock host_lock(host_mutex_);
    host_event_.wait_for(host_lock,10ms);
    now = SparkplugHelper::NowMs();
    const bool timeout = now >= timer;

    switch (work_state_) {
      case WorkState::Idle: {
        if (!timeout) { // Reconnect timeout
          break;
        }
        timer = now + 10'000; // Next try in 10 seconds
        work_state_ = WorkState::WaitOnConnect;
        const bool connect = SendConnect();
        if (!connect) {
          work_state_ = WorkState::Idle;
          break;
        }
        break;
      }

      case WorkState::WaitOnConnect: {
        if (timeout) {
          work_state_ = WorkState::Idle;
          timer = now + 10'000;
          break;
        }
        if (!IsConnected()) {
          break;
        }
        // Connected. Start subscriptions.
        // Add the host ID subscription last in list.
        auto* state_topic = GetTopicByMessageType("STATE");
        if (state_topic == nullptr) {
          work_state_ = WorkState::Idle;
          timer = now + 10'000;
          break;
        }
        AddSubscriptionByTopic(state_topic->Topic());
        StartSubscription();
        timer = now + 5'000;
        if (InService()) {
          PublishState(true);
          work_state_ = WorkState::WaitOnline;
        } else {
          PublishState(false);
          work_state_ = WorkState::WaitOffline;
        }
        break;
      }

      case WorkState::WaitOnline:
        if (stop_work_task_) {
          PublishState(false); // Send OFFLINE
          timer = now + 5'000;
          work_state_ = WorkState::WaitOffline;
        } else if (timeout) {
          LOG_ERROR() << "publish ONLINE failed";
          work_state_ = WorkState::Offline;
        } else if (IsDelivered()) {
          work_state_ = WorkState::Online;
        }
        break;

      case WorkState::Online:
        if (stop_work_task_) {
          PublishState(false);
        } else if (!InService()) {
          timer = now + 5'000;
          PublishState(false);
          work_state_ = WorkState::WaitOffline;
        }
        break;

      case WorkState::WaitOffline:
        if (timeout) {
          LOG_ERROR() << "publish OFFLINE failed (timeout) " << next_token_ << "/" << last_token_;
          work_state_ = WorkState::Offline;
        } else if (IsDelivered()) {
          work_state_ = WorkState::Offline;
        }
        break;

      case WorkState::Offline:
        if (stop_work_task_) {
          timer = now + 5'000;
          SendDisconnect();
          work_state_ = WorkState::WaitOnDisconnect;
        } else if (InService()) {
          timer = now + 5'000;
          PublishState(true);
          work_state_ = WorkState::WaitOnline;
        }
        break;

      case WorkState::WaitOnDisconnect:
        if (timeout || IsDelivered() ) {
          timer = now + 10'000; // Retry in 10s
          work_state_ = WorkState::Idle;
        }
        break;

      default: // Error
        work_state_ = WorkState::Idle;
        break;
    }
  }
  if (work_state_ != WorkState::Idle) {
    if (!IsConnected()) {
      if (listen_ && listen_->IsActive() ) {
        listen_->ListenText("Stop ignored due to not connected to server");
      }
    } else {
      if (listen_ && listen_->IsActive() ) {
        listen_->ListenText("Disconnecting");
      }
      if (work_state_ != WorkState::WaitOnDisconnect) {
        SendDisconnect();
      }
      for (size_t timeout = 0;
          !IsDelivered() && timeout < 1'000;
           ++timeout) {
        std::this_thread::sleep_for(10ms);
      }

      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Disconnected");
      }
    }
  }

}


bool SparkplugHost::SendConnect() {
  auto* state_topic = GetTopicByMessageType("STATE");
  if (state_topic == nullptr) {
    LOG_ERROR() << "No STATE topic found. Internal error.";
    return false;
  }
  const auto& payload = state_topic->GetPayload();

  MQTTAsync_willOptions will_options = MQTTAsync_willOptions_initializer;
  will_options.topicName = state_topic->Topic().c_str();
  will_options.message = nullptr; // Sending binary data
  will_options.retained = state_topic->Retained() ? 1 : 0;
  will_options.qos = static_cast<int>(state_topic->Qos());
  const auto& body = payload.Body();
  will_options.payload.len = static_cast<int>(body.size());
  will_options.payload.data = body.data();


  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.keepAliveInterval = 60; // 60 seconds between keep alive messages
  connect_options.cleansession = MQTTASYNC_TRUE;
  connect_options.connectTimeout = 10; // Wait max 10 seconds on connect.
  connect_options.onSuccess = OnConnect;
  connect_options.onFailure = OnConnectFailure;
  connect_options.context = this;

  server_session_ = -1; // Set the session to unknown.

  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to connect to the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(connect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }
    if (!IsFaulty()) {
      LOG_ERROR() << err.str();
    }
    SetFaulty(true, err.str());

    return false;
  }
  return true;
}

void SparkplugHost::PublishState(bool online) {
  auto* state_topic = GetTopicByMessageType("STATE", true);
  if (state_topic != nullptr) {
    auto& payload = state_topic->GetPayload();
    payload.Timestamp(SparkplugHelper::NowMs());
    payload.SetValue("online", online);
    state_topic->DoPublish();
  } else {
    LOG_ERROR() << "No STATE message defined. Internal error";
  }
}

bool SparkplugHost::IsOnline() const {
  return work_state_ == WorkState::Online;
}
bool SparkplugHost::IsOffline() const {
  return work_state_ == WorkState::Offline;
}

} // pub_sub