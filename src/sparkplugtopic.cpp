/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "sparkplugtopic.h"
#include "MQTTAsync.h"
#include "util/logstream.h"
#include "sparkplugnode.h"

#include <array>
#include <algorithm>

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
  options.onSuccess = nullptr; // OnSend;
  options.onFailure = OnSendFailure; // Note it just logging
  options.context = this;
  // options.token = parent_.GetUniqueToken();


  auto* listen = parent_.Listen();
  if (listen != nullptr && listen->IsActive() && listen->LogLevel() == 3) {
    const auto json = GetPayload().MakeJsonString();
    const auto& topic_name  = SparkplugTopic::Topic();
    listen->ListenText("Publish: %s: %s, %d",
                       Topic().c_str(), json.c_str(), options.token );
  }
  const auto send = MQTTAsync_sendMessage(parent_.Handle(), Topic().c_str(), &message, &options );
  if (send != MQTTASYNC_SUCCESS) {
    if (listen != nullptr && listen->IsActive()) {
      listen->ListenText("Publish Fail: %s", Topic().c_str());
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
  if (topic != nullptr && response != nullptr) {
    topic->SendComplete(*response);
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

bool SparkplugTopic::IsValidMessageType() const {
  constexpr std::array<std::string_view, 8> valid_message_types = {
      "NBIRTH", "NDEATH", "DBIRTH", "DDEATH", "NDATA", "NCMD", "DCMD", "STATE"
  };
  return std::any_of(valid_message_types.cbegin(), valid_message_types.cend(),
                     [&] (const auto& type) -> bool {
    return MessageType() == type;
  });
}

bool SparkplugTopic::IsBirthMessageType() const {
  constexpr std::array<std::string_view, 3> birth_message_types = {
      "NBIRTH", "DBIRTH", "STATE"
  };
  return std::any_of(birth_message_types.cbegin(), birth_message_types.cend(),
                     [&] (const auto& type) -> bool {
                       return MessageType() == type;
                     });
}

void SparkplugTopic::SendComplete(const MQTTAsync_successData &response) {
  auto* listen = parent_.Listen();
  if (listen != nullptr && listen->IsActive()) {
    listen->ListenText("Send completed. Token: %d", response.token);
  }
}

} // pub_sub