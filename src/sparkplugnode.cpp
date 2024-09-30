/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */


#include "sparkplugnode.h"
#include <chrono>
#include "util/utilfactory.h"
#include "util/logstream.h"
#include "util/stringutil.h"
#include "sparkplugtopic.h"
#include "sparkplugdevice.h"
#include "pubsub/pubsubfactory.h"
#include "sparkplughost.h"
#include "payloadhelper.h"

using namespace util::log;
using namespace util::string;
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

constexpr std::string_view kState = "STATE";

constexpr std::string_view kNodeBirth = "NBIRTH";
constexpr std::string_view kNodeDeath = "NDEATH";
constexpr std::string_view kNodeCommand = "NCMD";
constexpr std::string_view kNodeData = "NDATA";

constexpr std::string_view kDeviceBirth = "DBIRTH";
constexpr std::string_view kDeviceDeath = "DDEATH";
constexpr std::string_view kDeviceCommand = "DCMD";
constexpr std::string_view kDeviceData = "DDATA";

bool IsSparkplugHost(const pub_sub::IPubSubClient* client) {
  const auto* host = dynamic_cast<const pub_sub::SparkplugHost*>(client);
  return host != nullptr;
}

bool IsSparkplugNode(const pub_sub::IPubSubClient* client) {
  const auto* node = dynamic_cast<const pub_sub::SparkplugNode*>(client);
  return node != nullptr ? !node->GroupId().empty() && !node->Name().empty() : false;
}

bool IsSparkplugDevice(const pub_sub::IPubSubClient* client) {
  const auto* device = dynamic_cast<const pub_sub::SparkplugDevice*>(client);
  return device != nullptr;
}

}
namespace pub_sub {

SparkplugNode::SparkplugNode()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))) {
  CreateNodeBirthTopic();
  CreateNodeDeathTopic();
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

void SparkplugNode::OnSubscribe(void *, MQTTAsync_successData *) {
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
  switch (server_version_) {

    case MQTTVERSION_3_1:
      Version(ProtocolVersion::Mqtt31);
      break;

    case MQTTVERSION_5:
      Version(ProtocolVersion::Mqtt311);
      break;

    case MQTTVERSION_3_1_1:
    case MQTTVERSION_DEFAULT:
    default:
      Version(ProtocolVersion::Mqtt311);
      break;
  }

  server_session_ = info.sessionPresent;

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Connected. URI: %s, Version: %d, Session: %d",
                        server_uri_.c_str(), server_version_, server_session_);
  }
  LOG_INFO() << "Connected. Server: " << server_uri_;


  SetDelivered();

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
  LOG_ERROR() << err.str();

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetDelivered();
  node_event_.notify_one();
}

void SparkplugNode::ConnectionLost(const std::string& reason) {
  std::ostringstream err;
  err << "Connection lost. Reason: " << reason;
  LOG_INFO() << err.str(); // Not an error. This is a normal event

  SetConnectionLost();
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

  // I need to find the STATE,NBIRTH or DBIRTH topic for each message

  // Create a temporary topic that is used to parse the topic name.
  SparkplugTopic temp_topic(*this);
  temp_topic.Topic(topic_name);
  const std::string& message_type = temp_topic.MessageType();
  if (listen_ && listen_->IsActive()) {
    // Using the temp topic to parse the payload
    try {
      Payload &payload = temp_topic.GetPayload();
      auto &body = payload.Body();
      body.resize(message.payloadlen);
      memcpy(body.data(), message.payload, body.size());

      if (temp_topic.MessageType() == kState) {
        // Payload is JSON
        listen_->ListenText("Message Topic: %s\n%s", topic_name.c_str(), payload.BodyToString().c_str());
      } else {
        //Payload is protobuf
        PayloadHelper helper(payload);
        listen_->ListenText("Message Topic: %s\n%s", topic_name.c_str(), helper.DebugProtobuf().c_str());
      }

    } catch (const std::exception& err) {
      listen_->ListenText("Message Topic: %s Parse Error: %s", topic_name.c_str(), err.what());
    }
  }
  const auto& group_name = temp_topic.GroupId();
  const auto& node_name = temp_topic.NodeId();
  const auto& device_name = temp_topic.DeviceId();

  if (message_type == kState ) {
    HandleStateMessage(node_name, message);
  } else if (message_type == kNodeBirth) {
    HandleNodeBirthMessage(group_name, node_name, message);
  } else if (message_type == kNodeDeath) {
    HandleNodeDeathMessage(group_name, node_name, message);
  } else if (message_type == kNodeCommand) {
    HandleNodeCommandMessage(group_name, node_name, message);
  } else if (message_type == kNodeData) {
    HandleNodeDataMessage(group_name, node_name, message);
  } else if (message_type == kDeviceBirth) {
    HandleDeviceBirthMessage(group_name, node_name, device_name, message);
  } else if (message_type == kDeviceDeath) {
    HandleDeviceDeathMessage(group_name, node_name, device_name, message);
  } else if (message_type == kDeviceCommand) {
    HandleDeviceCommandMessage(group_name, node_name, device_name, message);
  } else if (message_type == kDeviceData) {
    HandleDeviceDataMessage(group_name, node_name, device_name, message);
  }

  /*
  auto* birth = GetTopicByMessageType(kNodeBirth.data());
  if (birth == nullptr) {
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
  */


  // Parse out the payload data and optional create the metrics.
  // topic->ParsePayloadData();

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

  AddDefaultMetrics();


  // Set the publishing flag to true. This defines that these messages should not be
  // updated from a subscription.
  if (auto* birth_topic = GetTopicByMessageType(kNodeBirth.data());
      birth_topic != nullptr ) {
    std::ostringstream topic_name;
    topic_name << kNamespace << "/" << GroupId() << "/" << kNodeBirth << "/" << Name();
    birth_topic->Topic(topic_name.str());
    birth_topic->Publish(true);
  }
  if (auto* death_topic = GetTopicByMessageType(kNodeDeath.data());
      death_topic != nullptr ) {
    std::ostringstream topic_name;
    topic_name << kNamespace << "/" << GroupId() << "/" << kNodeDeath << "/" << Name();
    death_topic->Publish(true);
  }

  // Need to keep track of my active host
  std::ostringstream my_hosts;
  my_hosts << kNamespace << "/STATE/#";
  AddSubscriptionFront(my_hosts.str());
  /*
  // Need to keep track of my device commands
  if (!device_list_.empty()) {
    std::ostringstream my_device_commands;
    my_device_commands << kNamespace << "/" << GroupId() << "/DCMD/" << Name() << "/#";
    AddSubscriptionFront(my_device_commands.str());
  }
  */
  // Add subscription on this node commands
  std::ostringstream my_node_commands;
  my_node_commands << kNamespace << "/" << GroupId() << "/NCMD/" << Name() << "/#";
  AddSubscriptionFront(my_node_commands.str());


  // Set alias numbers to all metrics.
  AssignAliasNumbers();

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
  payload.SetValue(kBdSeq.data(),bd_sequence_number_);
  payload.Timestamp(SparkplugHelper::NowMs());
  payload.SequenceNumber(0);
  payload.GenerateProtobuf();
  const auto& body = payload.Body();

  MQTTAsync_willOptions will_options = MQTTAsync_willOptions_initializer;
  will_options.retained = MQTTASYNC_TRUE;
  will_options.topicName = node_death->Topic().c_str();
  will_options.message = nullptr; // If message is null, the payload is sent instead.
  will_options.payload.data = body.data();
  will_options.payload.len = static_cast<int>(body.size());
  will_options.retained = 0; // Must be false
  will_options.qos = static_cast<int>(QualityOfService::Qos1); // Must be Qos1

  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.keepAliveInterval = 60; // 60 seconds between keep alive messages.
  connect_options.cleansession = MQTTASYNC_TRUE; // Must not have a persistent connection.
  connect_options.connectTimeout = 10; // Wait max 10 seconds on connect.
  connect_options.onSuccess = OnConnect;
  connect_options.onFailure = OnConnectFailure;
  connect_options.context = this;

  connect_options.automaticReconnect = 0; // No automatic reconnect
  connect_options.retryInterval = 0;

  connect_options.will = &will_options;

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
  topic->Publish(false);
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
  topic->Publish(false); // Is set to true if node is started
  topic->Qos(QualityOfService::Qos0);
  topic->Retained(false);

  auto& payload = topic->GetPayload();
  payload.Timestamp(SparkplugHelper::NowMs(), true);
}

void SparkplugNode::StartSubscription() {

  for (const std::string& topic : subscription_list_ ) {
    auto qos = DefaultQualityOfService();
    SparkplugTopic temp_topic(*this);
    temp_topic.Topic(topic);
    if (temp_topic.MessageType() == kNodeCommand) {
      qos = QualityOfService::Qos1; // Required by
    }

    MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;

    options.onSuccess = OnSubscribe;
    options.onFailure = OnSubscribeFailure;
    options.context = this;


    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("Subscribe: %s", topic.c_str());
    }
    const auto subscribe = MQTTAsync_subscribe(handle_, topic.c_str(),
                                               static_cast<int>(qos), &options);
    if (subscribe != MQTTASYNC_SUCCESS) {
      LOG_ERROR() << "Subscription Failed. Topic: " << topic << ". Error: " << MQTTAsync_strerror(subscribe);
    }
  }

}

ITopic *SparkplugNode::CreateTopic() {
  auto topic = std::make_unique<SparkplugTopic>(*this);
  std::scoped_lock list_lock(topic_mutex_);

  topic_list_.emplace_back(std::move(topic));
  return topic_list_.back().get();
}

ITopic *SparkplugNode::AddMetric(const std::shared_ptr<Metric> &value) {
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
  SetDelivered();
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

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetDelivered();
  node_event_.notify_one();
}

bool SparkplugNode::IsOnline() const {
  return node_state_ == NodeState::Online;
}

bool SparkplugNode::IsOffline() const {
  return node_state_ == NodeState::Idle;
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
  if (!InService() ) {
    return;
  }
  // Check if the node should wait for host online
  if (WaitOnHostOnline() && !IsHostOnline()) {
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
  if (stop_node_task_ || !InService() || (WaitOnHostOnline() && !IsHostOnline())) {
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

SparkplugHost *SparkplugNode::GetHost(const std::string &host_id) {

  try {
    // First check if this client is the requested host
    if (auto* my_host = dynamic_cast<SparkplugHost*>(this);
        my_host != nullptr && IEquals(host_id, my_host->Name())) {
      return my_host;
    }
    std::scoped_lock list_lock(list_mutex_);
    // Check remote hosts
    auto itr = std::find_if(node_list_.begin(), node_list_.end(),
                            [&] (auto& client) -> bool {
                              auto* host = dynamic_cast<SparkplugHost*>( client.get());
                              return host != nullptr && IEquals(host->Name(), host_id);
                            });
    return itr == node_list_.end() ? nullptr :
           dynamic_cast<SparkplugHost*>(itr->get());
  } catch (const std::exception& ) {

  }
  return nullptr;
}

SparkplugNode *SparkplugNode::GetNode(const std::string &group_id, const std::string &node_id) {
  if ( group_id.empty() || node_id.empty() ) {
    return nullptr;
  }

  // First check if this client is the requested host
  if (IEquals(group_id, GroupId()) && IEquals(node_id, Name()) ) {
    return this;
  }
  std::scoped_lock list_lock(list_mutex_);
  // Check remote nodes/hosts
  auto itr = std::find_if(node_list_.begin(), node_list_.end(),
                          [&] (auto& client) -> bool {
                            auto* node = client.get();
                            return node != nullptr && IEquals(group_id, node->GroupId() )
                                && IEquals(node->Name(), node_id);
                          });
  return itr == node_list_.end() ? nullptr : dynamic_cast<SparkplugNode*>(itr->get());
}


void SparkplugNode::HandleStateMessage(const std::string& host_name, const MQTTAsync_message& message) {
  if ( host_name.empty() ) {
    return;
  }

  // The Sparkplug node needs to keep a list of SCADA hosts.
  // If it doesn't exist, create it
  auto* host = GetHost(host_name);
  if (host == nullptr) {
    auto sparkplug_host = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugHost);
    if (sparkplug_host) {
      sparkplug_host->Name(host_name);
      {
        std::scoped_lock list_lock(list_mutex_);
        node_list_.emplace_back(std::move(sparkplug_host));
      }
      host = GetHost(host_name);
    }
  }

  if (host == nullptr) {
    LOG_ERROR() << "Failed to create a remote host. Host: " << host_name;
    return;
  }

  auto *state_topic = host->GetTopicByMessageType(kState.data());
  if (state_topic == nullptr || state_topic->Publish()) {
    return;
  }

  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = state_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugJson(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the STATE payload. Error: " << err.what();
  }
}

void SparkplugNode::HandleNodeBirthMessage(const std::string &group_name,
                                           const std::string &node_name,
                                           const MQTTAsync_message &message) {
  if ( group_name.empty() || node_name.empty() ) {
    return;
  }

  // The Sparkplug node have subscriptions on these node.
  // If the node doesn't exist, create it
  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    auto sparkplug_node = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugNode);
    if (sparkplug_node) {
      sparkplug_node->GroupId(group_name);
      sparkplug_node->Name(node_name);
      {
        std::scoped_lock list_lock(list_mutex_);
        node_list_.emplace_back(std::move(sparkplug_node));
      }
      node = GetNode(group_name, node_name);
    }
  }

  if (node == nullptr) {
    LOG_ERROR() << "Failed to create a remote node. Group/Node: "
      << group_name << "/" << node_name;
    return;
  }

  auto *birth_topic = node->GetTopicByMessageType(kNodeBirth.data());
  if (birth_topic == nullptr || birth_topic->Publish()) {
    return;
  }

  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = birth_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the NBIRTH  payload. Error: " << err.what();
  }

}
void SparkplugNode::HandleNodeDeathMessage(const std::string &group_name,
                                           const std::string &node_name,
                                           const MQTTAsync_message &message) {
  if (group_name.empty() || node_name.empty()) {
    return;
  }
  // The Sparkplug node have subscriptions on these node.
  // If the node doesn't exist, ignore the message
  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    return;
  }

  // The death topic is updated with the payload while the birth topic compare
  // the birt/death sequence number.
  auto *birth_topic = node->GetTopicByMessageType(kNodeBirth.data());
  auto *death_topic = node->GetTopicByMessageType(kNodeDeath.data());
  if (death_topic == nullptr || death_topic->Publish() || birth_topic == nullptr ) {
    // The message is handle by the node/device thread and polling of devices.
    return;
  }

    // Update the message payload. It's only the bdSeq number that needs to be cecked
  auto &payload = death_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the NDEATH payload. Error: " << err.what();
  }

  // Todo: Check that the NBIRTH and NDEATH bdSeqNo match. Otherwise ignore
  // Set all metrics as STALE (invalid).
  birth_topic->SetAllMetricsInvalid();
}

void SparkplugNode::HandleNodeCommandMessage(const std::string &group_name,
                                             const std::string &node_name,
                                             const MQTTAsync_message &message) {
  if (group_name.empty() || node_name.empty()) {
    return;
  }

  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    return;
  }
  // Fetch the NBIRTH topic that have all metrics
  auto *birth_topic = GetTopicByMessageType(kNodeBirth.data());
  // Note that the topic publish is the only node that should handle
  // the CMD. The node should not subscribe other node NCMD.
  if (birth_topic == nullptr || !birth_topic->Publish() ) {
    return;
  }

  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = birth_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(false);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the NCMD payload. Error: " << err.what();
  }

  // Todo: Handle any of the commands. Need to define what is a command.
}

void SparkplugNode::HandleNodeDataMessage(const std::string &group_name,
                                          const std::string &node_name,
                                          const MQTTAsync_message &message) {
  // The Sparkplug node have subscriptions on these node.
  // If the node doesn't exist, this means that the NBIRTH message
  // hasn't been received yet. Don't know which metrics that exist
  // on this node.
  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    // The NBIRTH has not been received yet. Ignore the message.
    // This is not an error.
    return;
  }
  auto *birth_topic = node->GetTopicByMessageType(kNodeBirth.data());

  // Update metrics if the topic not is publish.
  if (birth_topic == nullptr || birth_topic->Publish()) {
    // Do not update if the topic is marked as publish.
    // This is not an error.
    return;
  }

  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = birth_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(false); // Note that metrics must exist.
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the NDATA  payload. Error: " << err.what();
  }
}

void SparkplugNode::HandleDeviceBirthMessage(const std::string &group_name,
                                             const std::string &node_name,
                                             const std::string &device_name,
                                             const MQTTAsync_message &message) {
  if (group_name.empty() || node_name.empty() || device_name.empty() ) {
    return;
  }
  // The Sparkplug node have subscriptions on these node.
  // If the node doesn't exist, create it
  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    // Questionable if a node should be created here without NBIRTH message
    auto sparkplug_node = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugNode);
    if (sparkplug_node) {
      sparkplug_node->GroupId(group_name);
      sparkplug_node->Name(node_name);
      {
        std::scoped_lock list_lock(list_mutex_);
        node_list_.emplace_back(std::move(sparkplug_node));
      }
      node = GetNode(group_name, node_name);
    }
  }

  if (node == nullptr) {
    LOG_ERROR() << "Failed to create a remote node. Group/Node: "
                << group_name << "/" << node_name;
    return;
  }
  auto *nbirth_topic = node->GetTopicByMessageType(kNodeBirth.data());
  if (nbirth_topic == nullptr || nbirth_topic->Publish()) {
    // Ignore if this node publish data
    return;
  }

  auto* device = node->GetDevice(device_name);
  if (device == nullptr) {
    // Questionable if a node should be created here without NBIRTH message
   device = node->CreateDevice(device_name);
  }
  if (device == nullptr) {
    LOG_ERROR() << "Failed to create a device node. Group/Node/Device: "
                << group_name << "/" << node_name << "/" << device_name;
    return;
  }
  auto *dbirth_topic = device->GetTopicByMessageType(kDeviceBirth.data());
  if (dbirth_topic == nullptr || dbirth_topic->Publish()) {
    // Ignore if this device publish data
    return;
  }
  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = dbirth_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the DBIRTH  payload. Error: " << err.what();
  }
}

void SparkplugNode::HandleDeviceDeathMessage(const std::string &group_name,
                                             const std::string &node_name,
                                             const std::string &device_name,
                                             const MQTTAsync_message &message) {
  // Check the validity of the names.
  if (group_name.empty() || node_name.empty() || device_name.empty() ) {
    return;
  }

  // First get the node that holds the device object.
  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    // If the node doesn't exist, neither do the device.
    return;
  }

  auto* device = node->GetDevice(device_name);
  if (device == nullptr) {
    // If the device doesn't exist, then do nothing.
    return;
  }

  // Need both birth and death topic
  auto *birth_topic = device->GetTopicByMessageType(kDeviceBirth.data());
  auto *death_topic = device->GetTopicByMessageType(kDeviceDeath.data());
  if (birth_topic == nullptr || death_topic == nullptr || death_topic->Publish()) {
    // Ignore if this device publish data
    return;
  }

  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = death_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the DDEATH  payload. Error: " << err.what();
  }
  // Set all metrics in the device as STALE (invalid)
  birth_topic->SetAllMetricsInvalid();
}

void SparkplugNode::HandleDeviceCommandMessage(const std::string &group_name,
                                               const std::string &node_name,
                                               const std::string &device_name,
                                               const MQTTAsync_message &message) {
  if (group_name.empty() || node_name.empty() || device_name.empty() ) {
    return;
  }

  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    // Questionable if a node should be created here without NBIRTH message
    auto sparkplug_node = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugNode);
    if (sparkplug_node) {
      sparkplug_node->GroupId(group_name);
      sparkplug_node->Name(node_name);
      {
        std::scoped_lock list_lock(list_mutex_);
        node_list_.emplace_back(std::move(sparkplug_node));
      }
      node = GetNode(group_name, node_name);
    }
  }

  if (node == nullptr) {
    LOG_ERROR() << "Failed to create a remote node. Group/Node: "
                << group_name << "/" << node_name;
    return;
  }

  auto* device = node->GetDevice(device_name);
  if (device == nullptr) {
    // Need the DBIRTH message first, then it is possible to parse this message
    return;
  }

  auto *birth_topic = device->GetTopicByMessageType(kDeviceBirth.data());
  if (birth_topic == nullptr || birth_topic->Publish()) {
    // Ignore if this device publish data
    return;
  }
  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = birth_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the DBIRTH  payload. Error: " << err.what();
  }

  // Todo: Signal to check-up commands
}

void SparkplugNode::HandleDeviceDataMessage(const std::string &group_name,
                                            const std::string &node_name,
                                            const std::string &device_name,
                                            const MQTTAsync_message &message) {
  if (group_name.empty() || node_name.empty() || device_name.empty() ) {
    return;
  }

  auto* node = GetNode(group_name, node_name);
  if (node == nullptr) {
    // Questionable if a node should be created here without NBIRTH message
    auto sparkplug_node = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugNode);
    if (sparkplug_node) {
      sparkplug_node->GroupId(group_name);
      sparkplug_node->Name(node_name);
      {
        std::scoped_lock list_lock(list_mutex_);
        node_list_.emplace_back(std::move(sparkplug_node));
      }
      node = GetNode(group_name, node_name);
    }
  }

  if (node == nullptr) {
    LOG_ERROR() << "Failed to create a remote node. Group/Node: "
                << group_name << "/" << node_name;
    return;
  }

  auto* device = node->GetDevice(device_name);
  if (device == nullptr) {
    // Need the DBIRTH message first, then it is possible to parse this message
    return;
  }

  auto *birth_topic = device->GetTopicByMessageType(kDeviceBirth.data());
  if (birth_topic == nullptr || birth_topic->Publish()) {
    // Ignore if this device publish data
    return;
  }
  // Update the metrics. It is the online metrics that is of interest.
  auto &payload = birth_topic->GetPayload();
  auto &payload_data = payload.Body();
  try {
    const auto data_size = static_cast<size_t>(message.payloadlen);
    payload_data.resize(data_size);
    if (message.payload != nullptr && data_size > 0) {
      std::memcpy(payload_data.data(), message.payload, data_size);
      payload.ParseSparkplugProtobuf(true);
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to parse the DBIRTH  payload. Error: " << err.what();
  }
}

void SparkplugNode::AssignAliasNumbers() {
  uint64_t alias_number = 1;
  auto topic = GetTopicByMessageType("NBIRTH");
  if (topic != nullptr) {
    auto& payload = topic->GetPayload();
    auto& metric_list = payload.Metrics();
    for (auto& [name, metric] : metric_list) {
      if (metric) {
        metric->Alias(alias_number);
        ++alias_number;
      }
    }
  }

  for (auto& [device_name, device] : device_list_) {
    if (device) {
      auto* birth_topic = device->GetTopicByMessageType("DBIRTH");
      if (birth_topic != nullptr) {
        auto& payload = birth_topic->GetPayload();
        auto& metric_list = payload.Metrics();
        for (auto& [name, metric] : metric_list) {
          if (metric) {
            metric->Alias(alias_number);
            ++alias_number;
          }
        }
      }
    }
  }

  LOG_TRACE() << "Max alias number. Alias: " << alias_number;

}

void SparkplugNode::AddDefaultMetrics() {
  auto* topic = GetTopicByMessageType("NBIRTH");
  if (topic == nullptr) {
    return;
  }

  auto& payload = topic->GetPayload();
  auto bdSeq = payload.CreateMetric(kBdSeq.data());
  if (bdSeq) {
    bdSeq->Type(MetricType::UInt64);
    bdSeq->Value(bd_sequence_number_);
  }

  auto rebirth = payload.CreateMetric(kRebirth.data());
  if (rebirth) {
    rebirth->Type(MetricType::Boolean);
    rebirth->Value(false);
    rebirth->IsReadWrite(true);
  }

  auto reboot = payload.CreateMetric(kReboot.data());
  if (reboot) {
    reboot->Type(MetricType::Boolean);
    reboot->Value(false);
    reboot->IsReadWrite(true);
  }
  auto next_server = payload.CreateMetric(kNextServer.data());
  if (next_server) {
    next_server->Type(MetricType::Boolean);
    next_server->Value(false);
    next_server->IsReadWrite(true);
  }

  auto scan_rate = payload.CreateMetric(kScanRate.data());
  if (scan_rate) {
    scan_rate->Type(MetricType::Int64);
    scan_rate->Value(false);
    scan_rate->Unit("ms");
    scan_rate->IsReadWrite(true);
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

}

bool SparkplugNode::IsHostOnline() const {
  std::scoped_lock list_lock(list_mutex_);
  for ( const auto& node : node_list_) {
    if (!node || !IsSparkplugHost(node.get())) {
      continue;
    }
    auto* state_topic = node->GetTopicByMessageType("STATE");
    if (state_topic == nullptr) {
      continue;
    }
    const auto& payload = state_topic->GetPayload();
    const auto online = payload.GetValue<bool>("online");
    if (online) {
      return true;
    }
  }
  return false;
}

} // pub_sub