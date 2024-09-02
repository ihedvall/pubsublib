/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "sparkplugnode.h"
#include "util/utilfactory.h"
#include "util/logstream.h"
#include "sparkplugtopic.h"

using namespace util::log;

namespace pub_sub {

SparkplugNode::SparkplugNode()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))){

}

SparkplugNode::~SparkplugNode() {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Stopping client");
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
  host_event_.notify_one();
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
  server_session_ = 0;
  host_event_.notify_one();
}

void SparkplugNode::ConnectionLost(const std::string& reason) {
  std::ostringstream err;
  err << "Connection lost. Reason: " << reason;
  if (!IsFaulty()) {
    LOG_ERROR() << err.str();
  }
  SetFaulty(true, err.str());
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
  const auto qos = static_cast<QualityOfService>(message.qos);
  std::vector<uint8_t> payload(message.payloadlen, 0);
  if (message.payload != nullptr && message.payloadlen > 0) {
    std::memcpy(payload.data(), message.payload, message.payloadlen);
  }


  auto* topic = GetTopic(topic_name);
  if (topic == nullptr) {
    topic = CreateTopic();
    if (topic == nullptr) {
      LOG_ERROR() << "Failed to create topic. Internal error.";
      return;
    }
    topic->Topic(topic_name);
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Message Topic: %s: %s", topic_name.c_str(), topic->MessageType().c_str());
  }
  /*
  topic->Payload(payload);
  topic->Qos(qos);
  topic->Retained(message.retained == 1);
  const auto& value = topic->Value();
  if (value) {
    const auto now = TimeStampToNs(); // MQTT doesn't supply a network time so use the computer time.
    value->Timestamp(now);
    const auto text = topic->Payload<std::string>();
    value->Value(text);
    value->OnUpdate();
  }

  if (listen_ && listen_->IsActive() && listen_->LogLevel() != 1) {
    listen_->ListenText("Sub-Topic: %s, Value: %s", topic_name.c_str(),
                        topic->Payload<std::string>().c_str());
  }
   */
}

void SparkplugNode::DeliveryComplete(MQTTAsync_token token) {
  last_token_ = token - 1;
  if (last_token_ == next_token_) {
    host_event_.notify_one();
  }
  if (listen_ && listen_->IsActive() ) {
    listen_->ListenText("Token: %d", token);
  }
}

bool SparkplugNode::Start() {
   CreateNodeDeathTopic();
   CreateNodeBirthTopic();
   return true;
}

bool SparkplugNode::Stop() {
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

ITopic* SparkplugNode::CreateNodeDeathTopic() {
  auto* topic = GetTopicByMessageType("NDEATH");
  if (topic != nullptr) {
    return topic;
  }
/*
  std::ostringstream topic_name;
  topic_name << "spvBv1.0/" << GroupId() << "/NDEATH/" << NodeId();

  topic = CreateTopic();
  topic->Topic(topic_name.str());
  topic->Namespace("spvBv1.0");
  topic->GroupId("Group1");
  topic->MessageType("NDEATH");
  topic->NodeId("Node1");
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos1);
  topic->Retained(true);

  auto bdSeq = std::make_unique<IValue>();
  bdSeq->Name("bdSeq");
  bdSeq->Type(ValueType::UInt64);
  bdSeq->Value(birth_death_sequence_number);

  auto& payload = topic->GetPayload();
  payload.AddMetric(bdSeq);
  */
  return topic;
}

ITopic *SparkplugNode::CreateNodeBirthTopic() {
  auto* topic = GetTopicByMessageType("NBIRTH");
  if (topic != nullptr) {
    return topic;
  }
  /*
  std::ostringstream topic_name;
  topic_name << "spvBv1.0/" << GroupId() << "/NBIRTH/" << NodeId();

  topic = CreateTopic();
  topic->Topic(topic_name.str());
  topic->Namespace("spvBv1.0");
  topic->GroupId(GroupId());
  topic->MessageType("NBIRTH");
  topic->NodeId(NodeId());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos1);
  topic->Retained(true);

  auto bdSeq = std::make_unique<IValue>();
  bdSeq->Name("bdSeq");
  bdSeq->Type(ValueType::UInt64);
  bdSeq->Value(birth_death_sequence_number);

  auto& payload = topic->GetPayload();
  payload.AddMetric(bdSeq);
*/
  return topic;
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

ITopic *SparkplugNode::AddValue(const std::shared_ptr<IValue> &value) {
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
  next_token_ = GetUniqueToken(); // Used to detect delivery

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
    last_token_ = next_token_.load(); // No meaning to wait for delivery
  }
}

void SparkplugNode::Disconnect(MQTTAsync_successData&) {
  last_token_ = next_token_.load();
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
  last_token_ = next_token_.load();
}

bool SparkplugNode::IsOnline() const {
  return IsConnected();
}
bool SparkplugNode::IsOffline() const {
  return !IsConnected();
}

bool SparkplugNode::IsDelivered() {
  // return last_token_ == next_token_;
  return last_token_ >= next_token_;
}

} // pub_sub