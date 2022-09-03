/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include "util/utilfactory.h"
#include "util/logstream.h"
#include "sparkplugnode.h"

using namespace util::log;

namespace pub_sub {
SparkPlugNode::SparkPlugNode() {

}

SparkPlugNode::~SparkPlugNode() {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Stopping client");
  }
  SparkPlugNode::Stop();
}

bool SparkPlugNode::Start() {
   CreateNodeDeathTopic();
   CreateNodeBirthTopic();
   return MqttClient::Start();
}

bool SparkPlugNode::Stop() {
  return MqttClient::Stop();
}

bool SparkPlugNode::SendConnect() {
  auto* node_death = GetTopicByMessageType("NDEATH");
  if (node_death == nullptr) {
    LOG_ERROR() << "NDEATH topic is missing. Invalid use of function.";
    return false;
  }

  auto& payload = node_death->GetPayload();
  payload.GenerateFullBody();
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
  connect_options.onSuccess = MqttClient::OnConnect;
  connect_options.onFailure = MqttClient::OnConnectFailure;
  connect_options.context = this;
  connect_options.automaticReconnect = 1;
  connect_options.retryInterval = 10;
  connect_options.will = &will_options;

  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    LOG_ERROR()  << "Failed to connect to the MQTT broker. Error: " << MQTTAsync_strerror(connect);
    return false;
  }
  return true;
}

ITopic* SparkPlugNode::CreateNodeDeathTopic() {
  auto* topic = GetTopicByMessageType("NDEATH");
  if (topic != nullptr) {
    return topic;
  }

  std::ostringstream topic_name;
  topic_name << "spvBv1.0/" << GroupId() << "/NDEATH/" << NodeId();

  topic = CreateTopic();
  topic->Topic(topic_name.str());
  topic->Namespace("spvBv1.0");
  topic->GroupId(GroupId());
  topic->MessageType("NDEATH");
  topic->NodeId(NodeId());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos1);
  topic->Retained(true);

  auto bdSeq = std::make_unique<IMetric>();
  bdSeq->Name("bdSeq");
  bdSeq->Type(ValueType::UInt64);
  bdSeq->Value(birth_death_sequence_number);

  auto& payload = topic->GetPayload();
  payload.AddMetric(bdSeq);
  return topic;
}

ITopic *SparkPlugNode::CreateNodeBirthTopic() {
  auto* topic = GetTopicByMessageType("NBIRTH");
  if (topic != nullptr) {
    return topic;
  }
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

  auto bdSeq = std::make_unique<IMetric>();
  bdSeq->Name("bdSeq");
  bdSeq->Type(ValueType::UInt64);
  bdSeq->Value(birth_death_sequence_number);

  auto& payload = topic->GetPayload();
  payload.AddMetric(bdSeq);

  return topic;
}

} // pub_sub