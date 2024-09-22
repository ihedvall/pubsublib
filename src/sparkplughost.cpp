/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "sparkplughost.h"

#include <string_view>
#include <chrono>
#include "util/logstream.h"
#include "sparkplughelper.h"
#include <boost/json.hpp>

using namespace util::log;
using namespace std::chrono_literals;

namespace {
  constexpr std::string_view kNamespace = "spBv1.0";
  constexpr std::string_view kState = "STATE";
}

namespace pub_sub {
SparkplugHost::SparkplugHost()
: SparkplugNode() {
  CreateStateTopic();

}

SparkplugHost::SparkplugHost(const std::string &host_name)
: SparkplugNode() {
  GroupId("");
  Name(host_name);
  CreateStateTopic();

}

SparkplugHost::~SparkplugHost() {
  SparkplugHost::Stop(); // Just in case it have not stopped before
}

bool SparkplugHost::Start() {

  // Set the start time when starting the host.
  start_time_ = SparkplugHelper::NowMs();

  // Add the normal metrics as this host is publishing the STATE message.
  AddDefaultMetrics();

  if (auto* state_topic = GetTopicByMessageType(kState.data());
      state_topic != nullptr) {

    // Fix any changes of the host name after the creation.
    std::ostringstream topic_name;
    topic_name << kNamespace << "/" << kState << "/" << Name();
    state_topic->Topic(topic_name.str());

    // Set the publishing flag that shows that this host is publishing not subscribing
    state_topic->Publish(true); // This defines that this node will publish this topic.
  }

  // Add standard subscriptions
  std::ostringstream namespace_sub;
  namespace_sub << kNamespace << "/#";

  std::ostringstream state_sub;
  state_sub << kNamespace << "/" << kState << "/" << Name();

  // Note that we add first in the subscription list
  AddSubscriptionFront(state_sub.str());
  AddSubscriptionFront(namespace_sub.str());

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

  if (const auto create = MQTTAsync_create(&handle_, connect_string.str().c_str(),
                                       name_.c_str(),
                                       MQTTCLIENT_PERSISTENCE_NONE, nullptr);
      create != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to create the MQTT handle.";
    const auto* cause = MQTTAsync_strerror(create);
    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause;
    }
    LOG_ERROR() << err.str();
    return false;
  }

  ResetConnectionLost();
  ResetDelivered();
  // Setting up the callback. The OnDeliveryComplete is not set
  // as the function doesn't return the correct send token.
  if (const auto callback = MQTTAsync_setCallbacks(handle_, this,
                                               OnConnectionLost,
                                               OnMessageArrived,
                                               nullptr);
      callback != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to set the MQTT callbacks.";
    const auto* cause = MQTTAsync_strerror(callback);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Started Host: %s", Name().c_str());
  }

  // Create the worker task
  if (work_thread_.joinable() ) {
    work_thread_.join();
  }

  stop_work_task_ = false;
  work_thread_ = std::thread(&SparkplugHost::HostTask, this);
  node_event_.notify_one();
  return true;
}

bool SparkplugHost::Stop() {
  stop_work_task_ = true;
  node_event_.notify_one();
  if (work_thread_.joinable()) {
    work_thread_.join();
  }
  MQTTAsync_destroy(&handle_);
  handle_ = nullptr;
  return true;
}

void SparkplugHost::CreateStateTopic() {
  // Check if the STATE topic already exists
  auto* topic = GetTopicByMessageType("STATE");
  if (topic != nullptr) {
    return;
  }

  // Create the state topic
  std::ostringstream topic_name;
  topic_name << kNamespace <<  "/" << kState << "/" << Name();

  topic = CreateTopic();
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to create the topic. Internal error.";
    return;
  }

  topic->Topic(topic_name.str()); // The name is also set when the node/host is started
  topic->Namespace(kNamespace.data());
  topic->GroupId("");
  topic->MessageType("STATE");
  topic->NodeId(Name());
  topic->DeviceId("");

  // The publishing flag is used to detect if this host publish its
  // STATE message or if it actually
  topic->Publish(false);
  topic->Qos(QualityOfService::Qos1);
  topic->Retained(true);
  topic->ContentType("application/json");

  // Need to add some default metrics here instead of the start.
  // The issue that the STATE payload has changed in the 3.0 from
  // a string (OFFLINE/ONLINE) to a JSON string.
  // At least the online metric is needed.
  auto& payload = topic->GetPayload();

  auto online = payload.CreateMetric("online");
  online->Type(MetricType::Boolean);
  online->Value(false); // Meaning OFFLINE by default

  // The timestamp is not necessary to add but it is mandatory
  // and set by the Timestamp() function. We add it here to
  // simplify the creation of JSON payload
  auto timestamp = payload.CreateMetric("timestamp");
  timestamp->Type(MetricType::UInt64);
  payload.Timestamp(start_time_);

  // Note: Adding other default metrics at start
}

void SparkplugHost::HostTask() {
  host_timer_ = SparkplugHelper::NowMs();
  work_state_ = WorkState::Idle;

  while (!stop_work_task_) {
    std::unique_lock host_lock(node_mutex_);
    node_event_.wait_for(host_lock, 100ms );

    switch (work_state_) {
      case WorkState::Idle:
        DoIdle();
        break;


      case WorkState::WaitOnConnect:
        DoWaitOnConnect();
        break;

      case WorkState::Online:
        DoOnline();
        break;


      case WorkState::Offline:
        DoOffline();
        break;

      case WorkState::WaitOnDisconnect:
        DoWaitOnDisconnect();
        break;

      default: // Error
        work_state_ = WorkState::Idle;
        break;
    }
  }

  // Try to make a controlled disconnect
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
  auto& payload = state_topic->GetPayload();
  payload.Timestamp(start_time_);
  payload.SetValue("online", false);
  // Format the metrics into a JSON string.
  payload.GenerateJson(); // Todo: Fix UTF8 OFFLINE string in case of Sparkplug B version < 3.0

  MQTTAsync_willOptions will_options = MQTTAsync_willOptions_initializer;
  will_options.topicName = state_topic->Topic().c_str();
  will_options.message = nullptr; // Sending binary data
  will_options.retained = 1;
  will_options.qos = static_cast<int>(QualityOfService::Qos1);

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

  ResetDelivered();
  ResetConnectionLost();

  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to connect to the MQTT broker. Broker: " << Broker();
    const auto* cause = MQTTAsync_strerror(connect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << ", Error: " << cause;
    }
    LOG_ERROR() << err.str();
    return false;
  }

  return true;
}

void SparkplugHost::PublishState(bool online) {
  auto* state_topic = GetTopicByMessageType("STATE");
  if (state_topic != nullptr) {
    auto& payload = state_topic->GetPayload();
    // payload.Timestamp(start_time_);
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

void SparkplugHost::DoIdle() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= host_timer_;

  if (!timeout) { // Reconnect timeout
    return;
  }
  host_timer_ = now + 10'000; // Next try in 10 seconds
  work_state_ = WorkState::WaitOnConnect;

  const bool connect = SendConnect();
  if (!connect) {
    work_state_ = WorkState::Idle;
  }
}

void SparkplugHost::DoWaitOnConnect() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= host_timer_;

  if (timeout) {
    work_state_ = WorkState::Idle;
    host_timer_ = now + 10'000;
    return;
  }
  if (!IsConnected() || !IsDelivered()) {
    return;
  }
  // Connected. Start subscriptions.
  // Add the host ID subscription last in list.
  auto* state_topic = GetTopicByMessageType("STATE");
  if (state_topic == nullptr) {
    work_state_ = WorkState::Idle;
    host_timer_ = now + 10'000;
    return;
  }

  // Need to update the MQTT version that was received with the connect reply.
  auto& payload = state_topic->GetPayload();
  payload.SetValue("Properties/MQTT Version", MqttVersion());

  AddSubscription(state_topic->Topic());
  StartSubscription();

  if (InService()) {
    PublishState(true);
    work_state_ = WorkState::Online;
  } else {
    PublishState(false);
    work_state_ = WorkState::Offline;
  }
}

void SparkplugHost::DoOnline() {
  const auto now = SparkplugHelper::NowMs();
  if (stop_work_task_) {
    PublishState(false);
    host_timer_ = now + 5'000;
    SendDisconnect();
    work_state_ = WorkState::WaitOnDisconnect;
  } else if (!InService()) {
    PublishState(false);
    work_state_ = WorkState::Offline;
  }
}

void SparkplugHost::DoOffline() {
  const auto now = SparkplugHelper::NowMs();
  if (stop_work_task_) {
    host_timer_ = now + 5'000;
    SendDisconnect();
    work_state_ = WorkState::WaitOnDisconnect;
  } else if (InService()) {
    PublishState(true);
    work_state_ = WorkState::Online;
  }
}

void SparkplugHost::DoWaitOnDisconnect() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= host_timer_;
  if (timeout || IsDelivered() ) {
    host_timer_ = now + 10'000; // Retry in 10s
    work_state_ = WorkState::Idle;
  }

}
IPubSubClient *SparkplugHost::CreateDevice(const std::string &) {
  return nullptr;
}

void SparkplugHost::DeleteDevice(const std::string &) {
}

IPubSubClient *SparkplugHost::GetDevice(const std::string &device_name) {
  return nullptr;
}
const IPubSubClient *SparkplugHost::GetDevice(const std::string &device_name) const {
  return nullptr;
}

void SparkplugHost::AddDefaultMetrics() {

  auto* topic = GetTopicByMessageType("STATE");
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to find the STATE topic.";
    return;
  }

  auto& payload = topic->GetPayload();

  if (!HardwareMake().empty()) {
    auto hardware_make = payload.CreateMetric("Properties/Hardware Make");
    hardware_make->Type(MetricType::String);
    hardware_make->Value(HardwareMake());
  }

  if (!HardwareModel().empty()) {
    auto hardware_model = payload.CreateMetric("Properties/Hardware Model");
    hardware_model->Type(MetricType::String);
    hardware_model->Value(HardwareModel());
  }

  if (!OperatingSystem().empty()) {
    auto operating_system = payload.CreateMetric("Properties/OS");
    operating_system->Type(MetricType::String);
    operating_system->Value(OperatingSystem());
  }

  if (!OsVersion().empty()) {
    auto os_version = payload.CreateMetric("Properties/OS Version");
    os_version->Type(MetricType::String);
    os_version->Value(OsVersion());
  }

  auto sparkplug_version = payload.CreateMetric("Properties/Sparkplug Version");
  sparkplug_version->Type(MetricType::String);
  sparkplug_version->Value(SparkplugVersion());

  auto mqtt_version = payload.CreateMetric("Properties/MQTT Version");
  mqtt_version->Type(MetricType::String);
  mqtt_version->Value(MqttVersion());

}

} // pub_sub