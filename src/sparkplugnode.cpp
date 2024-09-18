/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */


#include "sparkplugnode.h"
#include <chrono>
#include "util/utilfactory.h"
#include "util/logstream.h"
#include "sparkplugtopic.h"
#include "sparkplugdevice.h"

using namespace util::log;
using namespace std::chrono_literals;

namespace {

constexpr std::string_view kNamespace = "spBv1.0";
constexpr std::string_view kBdSeq = "bdSeq";      ///< Birth/Death sequence number
constexpr std::string_view kReboot = "Node Control/Reboot";      ///< Reboot command
constexpr std::string_view kRebirth = "Node Control/Rebirth";    ///< Rebirth command
constexpr std::string_view kNextServer = "Node Control/Next Server";  ///< Step to next server command
constexpr std::string_view kScanRate = "Node Control/Scan Rate"; ///< Scan rate in ms

constexpr std::string_view kHardwareMake = "Properties/Hardware Make";
constexpr std::string_view kHardwareModel = "Properties/Hardware Model";
constexpr std::string_view kOs = "Properties/OS";
constexpr std::string_view kOsVersion = "Properties/OS Version";

}
namespace pub_sub {

SparkplugNode::SparkplugNode()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))) {
}

SparkplugNode::~SparkplugNode() {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Stopping Node: %s", Name().c_str());
  }
  SparkplugNode::Stop();
  listen_.reset();
}


void SparkplugNode::OnConnectionLost(void *context, char *cause) {
  auto *node = reinterpret_cast<SparkplugNode *>(context);
  std::string reason = cause != nullptr ? cause : "";
  if (node != nullptr) {
    node->ConnectionLost(reason);
  }
  if (cause != nullptr) {
    MQTTAsync_free(cause);
  }
}

int SparkplugNode::OnMessageArrived(void* context, char* topic_name,
                                    int topicLen, MQTTAsync_message* message) {
  auto *node = reinterpret_cast<SparkplugNode *>(context);
  const std::string topic_id = topic_name != nullptr && topicLen > 0 ? topic_name : "";
  if (node != nullptr && message != nullptr && !topic_id.empty()) {
    node->Message(topic_id, *message);
  }

  if (topic_name != nullptr) {
    MQTTAsync_free(topic_name);
  }
  if (message != nullptr) {
    MQTTAsync_freeMessage(&message);
  }
  return MQTTASYNC_TRUE;
}

void SparkplugNode::OnDeliveryComplete(void *context, MQTTAsync_token token) {
  auto *node = reinterpret_cast<SparkplugNode *>(context);
  if (node != nullptr) {
    node->DeliveryComplete(token);
  }
}

void SparkplugNode::OnConnect(void* context, MQTTAsync_successData* response) {
  auto *node = reinterpret_cast<SparkplugNode *>(context);
  if (node != nullptr && response != nullptr) {
    node->Connect(*response);
  }
}

void SparkplugNode::OnConnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *node = reinterpret_cast<SparkplugNode *>(context);
  if (node != nullptr && response != nullptr) {
    node->ConnectFailure(*response);
  }
}

void SparkplugNode::OnSubscribeFailure(void *context, MQTTAsync_failureData *response) {
  auto *node = reinterpret_cast<SparkplugNode *>(context);
  if (node != nullptr && response != nullptr) {
    node->SubscribeFailure(*response);

  }
}

void SparkplugNode::OnSubscribe(void *, MQTTAsync_successData *response) {
}

void SparkplugNode::OnDisconnect(void* context, MQTTAsync_successData* response) {
  auto *node = reinterpret_cast<pub_sub::SparkplugNode *>(context);
  if (node != nullptr && response != nullptr) {
    node->Disconnect(*response);
  }
}

void SparkplugNode::OnDisconnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *node = reinterpret_cast<pub_sub::SparkplugNode *>(context);
  if (node != nullptr && response != nullptr) {
    node->DisconnectFailure(*response);
  }
}

void SparkplugNode::Connect(const MQTTAsync_successData &response) {
  const auto& info = response.alt.connect;
  server_uri_ = info.serverURI != nullptr ? info.serverURI : std::string();
  server_version_ = info.MQTTVersion;
  server_session_ = info.sessionPresent;

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Connected. URI: %s, Version: %d, Session: %d",
                        server_uri_.c_str(), server_version_, server_session_);
  }
  if (IsFaulty()) {
    LOG_INFO() << "Connected. Server: " << server_uri_;
  }
  SetFaulty(false,{});

  ResetDelivered();

  node_event_.notify_one();
}

void SparkplugNode::ConnectFailure(const MQTTAsync_failureData &response) {
  std::ostringstream err;
  err << "Failed to connect to the MQTT broker.";
  const auto* cause = MQTTAsync_strerror(response.code);
  if (cause != nullptr && strlen(cause) > 0) {
    err << " Error: " << cause << ".";
  }
  if (response.message != nullptr) {
    err << " Message: " << response.message;
  }
  if (!IsFaulty()) {
    LOG_ERROR() << err.str();
  }
  SetFaulty(true, err.str());
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }

  node_event_.notify_one();
}

void SparkplugNode::ConnectionLost(const std::string& reason) {
  std::ostringstream err;
  err << "Connection lost. Reason: " << reason;
  if (!IsFaulty()) {
    LOG_ERROR() << err.str();
  }
  SetFaulty(true, err.str());
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
}

void SparkplugNode::SubscribeFailure(MQTTAsync_failureData &response) {
  std::ostringstream err;
  err << "Subscribe Failure. Error: " << MQTTAsync_strerror(response.code);
  if (response.message != nullptr) {
    err << ". Message: " << response.message;
  }
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str() );
  }
  LOG_ERROR() << err.str();
}


void SparkplugNode::Message(const std::string& topic_name, const MQTTAsync_message& message) {
  if (message.payloadlen < 0) {
    LOG_ERROR() << "Invalid payload length. Length: " << message.payloadlen;
    return;
  }
  const auto data_size = static_cast<size_t>(message.payloadlen);

  auto* topic = GetTopic(topic_name);
  if (topic == nullptr) {
    // The topic doesn't exist. If I receive this message, I have
    // set up a subscription on this message. Create a topic
    // for the incoming. However, it is only the xBIRTH messages
    // that can create metrics if they are missing.
    topic = CreateTopic();
    if (topic == nullptr) {
      LOG_ERROR() << "Failed to create topic. Internal error.";
      return;
    }
    topic->Topic(topic_name);
    topic->Publish(false); // This is a subscription
    topic->Qos(static_cast<QualityOfService>(message.qos));
    if (topic->MessageType() == "STATE") {
      topic->ContentType("application/json");
    } else {
      topic->ContentType("application/protobuf");
    }
    topic->Retained(message.retained != 0);

  }

  // Copy the message data to the topics payload data.
  auto& payload = topic->GetPayload();
  auto& payload_data = payload.Body();

  try {
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
    }
  } catch (const std::exception& err) {
    LOG_ERROR() << "Failed to create temporary payload buffer. Error: " << err.what();
    return;
  }

  // Check if I am publishing this topic. If so I should not
  // update my values. I can however generate a listen debug output.
  if (topic->Publish()) {
    if (listen_ && listen_->IsActive()) {
      if (topic->MessageType() == "STATE") {
        // JSON data
        listen_->ListenText("Message Topic: %s: %s", topic_name.c_str(),
                            payload.BodyToString().c_str());
      } else {
        // Protobuf data.
      }
    }
    return;
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Message Topic: %s: %s", topic_name.c_str(), topic->MessageType().c_str());
  }

  // Parse out the payload data and optional create the metrics.
  topic->ParsePayloadData();

}

void SparkplugNode::DeliveryComplete(MQTTAsync_token token) {
 delivered_ = true;

 node_event_.notify_one();

 if (listen_ && listen_->IsActive() ) {
    listen_->ListenText("Delivered Token: %d", token);
  }
}

bool SparkplugNode::Start() {
  // Create the worker task
  if (work_thread_.joinable()) {
    work_thread_.join();
  }
  if (GroupId().empty()) {
    LOG_ERROR() << "There is no group ID defined. Cannot start the node. Node: " << Name();
    return false;
  }
  stop_node_task_ = false;
  work_thread_ = std::thread(&SparkplugNode::NodeTask, this);

  node_event_.notify_one();

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Started Node: %s", Name().c_str());
  }
  return true;
}

bool SparkplugNode::Stop() {
  stop_node_task_ = true;
  node_event_.notify_one();
  if (work_thread_.joinable()) {
    work_thread_.join();
  }
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }
  return true;
}

bool SparkplugNode::CreateNode() {

  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

  // Add subscription on my commands
  std::ostringstream my_node_commands;
  my_node_commands << kNamespace << "/" << GroupId() << "/NCMD/" << Name() << "/#";
  AddSubscriptionByTopic(my_node_commands.str());

  std::ostringstream my_device_commands;
  my_device_commands << kNamespace << "/" << GroupId() << "/DCMD/" << Name() << "/#";
  AddSubscriptionByTopic(my_device_commands.str());

  // Need to keep track of my active host
  std::ostringstream my_hosts;
  my_hosts << kNamespace << "/STATE/#";
  AddSubscriptionByTopic(my_hosts.str());

  CreateNodeDeathTopic();
  CreateNodeBirthTopic(); // The birth topic actually holds the nodes all metrics

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
                                               nullptr);
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
  return true;
}



bool SparkplugNode::SendConnect() {
  auto* node_death = GetTopicByMessageType("NDEATH");
  if (node_death == nullptr) {
    LOG_ERROR() << "NDEATH topic is missing. Invalid use of function.";
    return false;
  }

  auto& payload = node_death->GetPayload();
  payload.GenerateProtobuf();
  const auto& body = payload.Body();

  MQTTAsync_willOptions will_options = MQTTAsync_willOptions_initializer;
  will_options.retained = MQTTASYNC_TRUE;
  will_options.topicName = node_death->Topic().c_str();
  will_options.message = nullptr; // If message is null, the payload is sent instead.
  will_options.payload.data = body.data();
  will_options.payload.len = static_cast<int>(body.size());
  will_options.retained = node_death->Retained() ? 1 : 0;
  will_options.qos = static_cast<int>(node_death->Qos());

  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.keepAliveInterval = 60; // 60 seconds between keep alive messages
  connect_options.cleansession = MQTTASYNC_TRUE;
  connect_options.connectTimeout = 10; // Wait max 10 seconds on connect.
  connect_options.onSuccess = OnConnect;
  connect_options.onFailure = OnConnectFailure;
  connect_options.context = this;
  connect_options.automaticReconnect = 1;
  connect_options.retryInterval = 10;
  connect_options.will = &will_options;

  server_session_ = -1; // Used to indicate connect or connect failure
  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    LOG_ERROR()  << "Failed to connect to the MQTT broker. Error: " << MQTTAsync_strerror(connect);
    return false;
  }
  return true;
}

void SparkplugNode::CreateNodeDeathTopic() {
  auto* topic = GetTopicByMessageType("NDEATH");
  if (topic != nullptr) {
    // Need to delete the current topic and create a new
    // in case of any configuration change.
    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("Deleting previous NDEATH message");
    }
    DeleteTopic(topic->Topic());
  }

  std::ostringstream topic_name;
  topic_name << "spvBv1.0/" << GroupId() << "/NDEATH/" << Name();

  topic = CreateTopic();
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to create NDEATH topic.";
    return;
  }
  topic->Topic(topic_name.str());
  topic->Namespace("spvBv1.0");
  topic->GroupId(GroupId());
  topic->MessageType("NDEATH");
  topic->NodeId(Name());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos1);
  topic->Retained(false);

  auto& payload = topic->GetPayload();

  auto bdSeq = payload.CreateMetric(kBdSeq.data());
  if (bdSeq) {
    bdSeq->Type(MetricType::UInt64);
    bdSeq->Value(bd_sequence_number_);
  }
  payload.Timestamp(SparkplugHelper::NowMs(), true);
}

void SparkplugNode::CreateNodeBirthTopic() {
  auto* topic = GetTopicByMessageType("NBIRTH");
  if (topic != nullptr) {
    // Need to delete the current topic and create a new
    // in case of any configuration change.
    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("Deleting previous NBIRTH message");
    }
    DeleteTopic(topic->Topic());
  }

  std::ostringstream topic_name;
  topic_name << "spvBv1.0/" << GroupId() << "/NBIRTH/" << Name();

  topic = CreateTopic();
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to create the NBIRTH topic.";
    return;
  }

  topic->Topic(topic_name.str());
  topic->Namespace("spvBv1.0");
  topic->GroupId(GroupId());
  topic->MessageType("NBIRTH");
  topic->NodeId(Name());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos0);
  topic->Retained(false);

  auto& payload = topic->GetPayload();
  auto bdSeq = payload.CreateMetric(kBdSeq.data());
  if (bdSeq) {
    bdSeq->Type(MetricType::UInt64);
    bdSeq->Value(bd_sequence_number_);
  }

  auto reboot = payload.CreateMetric(kReboot.data());
  if (reboot) {
    reboot->Type(MetricType::Boolean);
    reboot->Value(false);
  }

  auto rebirth = payload.CreateMetric(kRebirth.data());
  if (rebirth) {
    rebirth->Type(MetricType::Boolean);
    rebirth->Value(false);
  }

  auto next_server = payload.CreateMetric(kNextServer.data());
  if (next_server) {
    next_server->Type(MetricType::Boolean);
    next_server->Value(false);
  }

  auto scan_rate = payload.CreateMetric(kScanRate.data());
  if (scan_rate) {
    scan_rate->Type(MetricType::Int64);
    scan_rate->Value(false);
    scan_rate->Unit("ms");
  }

  auto hardware_make = !HardwareMake().empty() ?  payload.CreateMetric(kHardwareMake.data()) : nullptr;
  if (hardware_make) {
    hardware_make->Type(MetricType::String);
    hardware_make->Value(HardwareMake());
  }

  auto hardware_model = !HardwareModel().empty() ?  payload.CreateMetric(kHardwareModel.data()) : nullptr;
  if (hardware_model) {
    hardware_model->Type(MetricType::String);
    hardware_model->Value(HardwareModel());
  }

  auto operating_system = !OperatingSystem().empty() ?  payload.CreateMetric(kOs.data()) : nullptr;
  if (operating_system) {
    operating_system->Type(MetricType::String);
    operating_system->Value(OperatingSystem());
  }

  auto os_version = !OsVersion().empty() ?  payload.CreateMetric(kOsVersion.data()) : nullptr;
  if (os_version) {
    os_version->Type(MetricType::String);
    os_version->Value(OsVersion());
  }

  payload.Timestamp(SparkplugHelper::NowMs(), true);
}

void SparkplugNode::StartSubscription() {
  for (const std::string& topic : subscription_list_ ) {
    MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
    options.onSuccess = OnSubscribe;
    options.onFailure = OnSubscribeFailure;
    options.context = this;

    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("Subscribe: %s", topic.c_str());
    }
    const auto subscribe = MQTTAsync_subscribe(handle_, topic.c_str(),
                                               static_cast<int>(DefaultQualityOfService()), &options);
    if (subscribe != MQTTASYNC_SUCCESS) {
      LOG_ERROR() << "Subscription Failed. Topic: " << topic << ". Error: " << MQTTAsync_strerror(subscribe);
    }
  }

}

ITopic *SparkplugNode::CreateTopic() {
  auto topic = std::make_unique<SparkplugTopic>(*this);
  std::scoped_lock list_lock(topic_mutex);

  topic_list_.emplace_back(std::move(topic));
  return topic_list_.back().get();
}

ITopic *SparkplugNode::AddMetric(const std::shared_ptr<IMetric> &value) {
  return nullptr;
}

bool SparkplugNode::IsConnected() const {
  return handle_ != nullptr && MQTTAsync_isConnected(handle_);
}
void SparkplugNode::SendDisconnect() {
  MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
  disconnect_options.onSuccess = OnDisconnect;
  disconnect_options.onFailure = OnDisconnectFailure;
  disconnect_options.context = this;
  disconnect_options.timeout = 5000;

  ResetDelivered();

  const auto disconnect = MQTTAsync_disconnect(handle_, &disconnect_options);
  if (disconnect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to disconnect from the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(disconnect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }
    SetFaulty(true, err.str());
    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("%s", err.str().c_str());
    }
    delivered_ = true; // No meaning to wait for delivery
    node_event_.notify_one();
  } else {
    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("Sent Disconnect. Node: %s", Name().c_str());
    }
  }
}

void SparkplugNode::Disconnect(MQTTAsync_successData&) {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Node disconnected. Node: %s", Name().c_str());
  }
  delivered_ = true;
  node_event_.notify_one();
}

void SparkplugNode::DisconnectFailure(MQTTAsync_failureData& response) {
  std::ostringstream err;
  err << "Disconnect failure.";
  const int code = response.code;
  const auto* cause = MQTTAsync_strerror(code);
  if (cause != nullptr && strlen(cause) > 0) {
    err << " Error: " << cause;
  }
  SetFaulty(true, err.str());
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  delivered_ = true;
  node_event_.notify_one();
}

bool SparkplugNode::IsOnline() const {
  return node_state_ == NodeState::Online;
}

bool SparkplugNode::IsOffline() const {
  return node_state_ == NodeState::Idle;
}

bool SparkplugNode::IsDelivered() {
  return delivered_;
}

void SparkplugNode::NodeTask() {
  node_timer_ = 0;
  node_state_ = NodeState::Idle;
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

  while (!stop_node_task_) {
    std::unique_lock node_lock(node_mutex_);
    node_event_.wait_for(node_lock, 100ms);

    switch (node_state_) {
      case NodeState::Idle: // Wait for in-service command
        DoIdle();
        break;

      case NodeState::WaitOnConnect: // Wait for in-service command
        DoWaitOnConnect();
        break;

      case NodeState::Online:
        DoOnline();
        break;

      case NodeState::WaitOnDisconnect:
        DoWaitOnDisconnect();
        break;

      default: // Invalid/Unknown state
        node_timer_ = SparkplugHelper::NowMs() + 10'000;
        node_state_ = NodeState::Idle;
        break;
    }
  }
  // Need to send disconnect or wait on the disconnect
  if (node_state_ != NodeState::Idle) {
    if (!IsConnected()) {
      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Stop ignored due to not connected to server");
      }
    } else {
      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Disconnecting");
      }
      if (node_state_ != NodeState::WaitOnDisconnect) {
        SendDisconnect();
      }
      // Wait for 5s for the disconnect to be delivered
      for (size_t timeout = 0;
           !IsDelivered() && timeout < 500;
           ++timeout) {
        std::this_thread::sleep_for(10ms);
      }

      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Disconnected");
      }
    }
  }
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }
}

void SparkplugNode::DoIdle() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= node_timer_;

  // Destroy any previously created context/handle.
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

  // Check the retry timeout first (10s)
  if (!timeout) {
    return;
  }
  // Check if in service
  if (!InService()) {
    return;
  }

  // In-service create a communication context/handle and connect
  // to the MQTT server.
  const auto create = CreateNode();
  if (!create) {
    node_timer_ = now + 10'000; // 10 second to next create try
    return;
  }

  const auto connect = SendConnect();
  if (!connect) {
    node_timer_ = now + 10'000; // 10 second to next create try
    return;
  }

  // Switch state and wait for connection
  node_timer_ = now + 5'000; // Wait 5 second for connection
  node_state_ = NodeState::WaitOnConnect;
}

void SparkplugNode::DoWaitOnConnect() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= node_timer_;

  // Connection timeout
  if (timeout && !IsConnected()) {
    node_timer_ = now + 10'000; // Retry in 10 seconds
    node_state_ = NodeState::Idle;
    return;
  }

  // Check if connected.
  if (!IsConnected()) {
    return;
  }

  // Start subscriptions and publish NBIRTH message.
  // We will not check that it is delivered.
  StartSubscription();
  PublishNodeBirth();
  node_state_ = NodeState::Online;
  PollDevices(); // Speed up the DBIRTH sending
}

void SparkplugNode::DoOnline() {
  const auto now = SparkplugHelper::NowMs();
  // const bool timeout = now >= node_timer_;
  if (stop_node_task_ || !InService()) {
    PollDevices(); // This generates DDEATH messages
    PublishNodeDeath();
    SendDisconnect();
    node_timer_ = now + 5'000;
    node_state_ = NodeState::WaitOnDisconnect;
  } else {
    PollDevices();
  }
}

void SparkplugNode::DoWaitOnDisconnect() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= node_timer_;
  if (timeout || IsDelivered() ) {
    node_timer_ = now + 10'000; // Retry in 10s
    node_state_ = NodeState::Idle;
  }
}

void SparkplugNode::PublishNodeBirth() {
  if (!IsConnected()) {
    return;
  }
  auto* birth_topic = GetTopicByMessageType("NBIRTH");
  if (birth_topic != nullptr) {
    auto& payload = birth_topic->GetPayload();
    payload.Timestamp(SparkplugHelper::NowMs());
    birth_topic->DoPublish();
  } else {
    LOG_ERROR() << "No NBIRTH message defined. Internal error";
  }
}

void SparkplugNode::PublishNodeDeath() {
  if (!IsConnected()) {
    return;
  }
  auto* death_topic = GetTopicByMessageType("NDEATH");
  if (death_topic != nullptr) {
    auto& payload = death_topic->GetPayload();
    payload.Timestamp(SparkplugHelper::NowMs());
    death_topic->DoPublish();
  } else {
    LOG_ERROR() << "No NDEATH message defined. Internal error";
  }
}

IPubSubClient *SparkplugNode::CreateDevice(const std::string &device_name) {
  if (device_name.empty()) {
    LOG_ERROR() << "Device name cannot be empty. Node: " << Name();
    return nullptr;
  }
  auto* device = GetDevice(device_name);
  if (device == nullptr) {
    auto new_device = std::make_unique<SparkplugDevice>(*this);
    new_device->GroupId(GroupId());
    new_device->Name(device_name);
    device_list_.emplace(device_name,std::move(new_device));
  }
  return GetDevice(device_name);
}

void SparkplugNode::DeleteDevice(const std::string &device_name) {
  auto itr = device_list_.find(device_name);
  if (itr != device_list_.end()) {
    device_list_.erase(itr);
  }
}

IPubSubClient *SparkplugNode::GetDevice(const std::string &device_name) {
  auto itr = device_list_.find(device_name);
  return  itr == device_list_.end() ? nullptr : itr->second.get();
}

const IPubSubClient *SparkplugNode::GetDevice(const std::string &device_name) const {
  const auto itr = device_list_.find(device_name);
  return itr == device_list_.cend() ? nullptr : itr->second.get();
}

void SparkplugNode::PollDevices() {
  for ( auto& [name, device] : device_list_ ) {
    if (device) {
      device->Poll();
    }
  }
}

} // pub_sub