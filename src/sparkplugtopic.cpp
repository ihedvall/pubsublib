/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "sparkplugtopic.h"
#include "MQTTAsync.h"
#include "util/logstream.h"
#include "sparkplugnode.h"

namespace {
  constexpr std::string_view kNamespace = "spBv1.0";
}

namespace pub_sub {
SparkplugTopic::SparkplugTopic(SparkplugNode& parent)
: ITopic(),
  parent_(parent) {
  Namespace(kNamespace.data());
}

void SparkplugTopic::DoPublish() {
  auto& payload = GetPayload();
  if (MessageType() == "STATE") {
    // Payload is a JSON string
    payload.GenerateJson();
  } else {
     // Payload is a protobuf data buffer
     payload.GenerateProtobuf();
  }

  auto& body = payload.Body();
  MQTTAsync_message  message = MQTTAsync_message_initializer;
  message.payload = body.data();
  message.payloadlen = static_cast<int>(body.size());
  message.qos = static_cast<int>(Qos());
  message.retained = Retained() ? 1 : 0;


  MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
  options.onSuccess = OnSend;
  options.onFailure = OnSendFailure;
  options.context = this;
  const auto token = parent_.GetUniqueToken();
  parent_.NextToken(token);
  options.token = token;

  auto* listen = parent_.Listen();
  if (listen != nullptr && listen->IsActive() && listen->LogLevel() == 3) {
    const auto json = GetPayload().MakeJsonString();
    const auto& topic_name  = SparkplugTopic::Topic();
    listen->ListenText("Publish: %s: %s, %d",
                       Topic().c_str(), json.c_str(), token );
  }
  const auto send = MQTTAsync_sendMessage(parent_.Handle(), Topic().c_str(), &message, &options );
  if (send != MQTTASYNC_SUCCESS) {
    if (listen != nullptr && listen->IsActive()) {
      listen->ListenText("Publish Fail: %s", Topic().c_str());
    }
  }
}

void SparkplugTopic::DoSubscribe() {
  MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
  options.onSuccess = OnSubscribe;
  options.onFailure = OnSubscribeFailure;
  options.context = this;

  auto* listen = parent_.Listen();
  if (listen != nullptr && listen->IsActive() && listen->LogLevel() == 3) {
    listen->ListenText("Subscribe: %s", Topic().c_str() );
  }
  const auto subscribe = MQTTAsync_subscribe(parent_.Handle(), Topic().c_str(),
                                             static_cast<int>(Qos()), &options);
  if (subscribe != MQTTASYNC_SUCCESS) {
    LOG_ERROR() << "Subscribe Failed. Topic: " << Topic()
      << ". Error: " << MQTTAsync_strerror(subscribe);
    if (listen != nullptr && listen->IsActive()) {
      listen->ListenText("Subscribe Fail. Topic: %s, Error: %s",
                         Topic().c_str(), MQTTAsync_strerror(subscribe));
    }
  }
}

void SparkplugTopic::OnSendFailure(void *context, MQTTAsync_failureData *response) {
  auto *topic = reinterpret_cast<SparkplugTopic *>(context);
  if (topic != nullptr) {
    auto* listen = topic->parent_.Listen();
    if (listen != nullptr && listen->IsActive() && response != nullptr) {
      listen->ListenText("Publish Send Failure: %s, Error: %s",
                        topic->Topic().c_str(),
                        MQTTAsync_strerror(response->code));
    }
  }
}

void SparkplugTopic::OnSend(void *context, MQTTAsync_successData *response) {
  auto *topic = reinterpret_cast<SparkplugTopic *>(context);
  if (topic != nullptr) {
  }
}

void SparkplugTopic::OnSubscribeFailure(void *context, MQTTAsync_failureData *response) {
  auto *topic = reinterpret_cast<SparkplugTopic *>(context);
  if (topic != nullptr) {
    auto* listen = topic->parent_.Listen();
    if (listen != nullptr && listen->IsActive() && response != nullptr) {
      listen->ListenText("Subscribe Failure: %s, Error: %s",
                         topic->Topic().c_str(),
                         MQTTAsync_strerror(response->code));
    }
  }
}

void SparkplugTopic::OnSubscribe(void *context, MQTTAsync_successData *response) {
  auto *topic = reinterpret_cast<SparkplugTopic *>(context);
  if (topic != nullptr) {
  }
}


} // pub_sub