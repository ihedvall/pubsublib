/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "mqtttopic.h"
#include "mqttclient.h"
#include <util/logstream.h>

using namespace util::log;

namespace pub_sub {
MqttTopic::MqttTopic(MqttClient &parent)
: parent_(parent) {
}

void MqttTopic::DoPublish() {
  if (!Publish()) {
    return;
  }
  // Generate the payload
  auto& payload = GetPayload();
  // Fill the body with MQTT
  if (IsJson()) {
    payload.GenerateJson();
  } else if (IsProtobuf()) {
    payload.GenerateProtobuf();
  } else {
    payload.GenerateText();
  }

 // lrv_ = PayloadBody<std::vector<uint8_t>>();
  auto& listen = parent_.Listen();
  if (listen.IsActive() && listen.LogLevel() != 2) {
    const auto text = payload.BodyToString();
    listen.ListenText("Publish: %s: %s", Topic().c_str(), text.c_str());
  }

  auto& body = payload.Body();
  MQTTAsync_message  message = MQTTAsync_message_initializer;
  message.payload = body.data();
  message.payloadlen = static_cast<int>(body.size());
  message.qos = static_cast<int>(Qos());
  message.retained = Retained() ? 1 : 0;

  MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
  if (parent_.Version() == ProtocolVersion::Mqtt5) {
    options.onFailure5 = OnSendFailure5;
  } else {
    options.onFailure = OnSendFailure;
  }
  options.context = this;

  const auto send = MQTTAsync_sendMessage(parent_.Handle(), Topic().c_str(), &message, &options );
  if (send != MQTTASYNC_SUCCESS) {
    SetAllMetricsInvalid();
    std::ostringstream err;
    err << "Failed to publish to the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(send);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }
    LOG_ERROR() << err.str();
  }

}

void MqttTopic::OnSendFailure(void *context, MQTTAsync_failureData *response) {
  auto *topic = reinterpret_cast<MqttTopic *>(context);
  if (topic != nullptr ) {
    topic->SetAllMetricsInvalid();

    auto& listen = topic->parent_.Listen();
    if (listen.IsActive() && response != nullptr) {
      listen.ListenText("Publish Failure. Topic:: %s, Error: %s",
                        topic->Topic().c_str(),
                        MQTTAsync_strerror(response->code));
    }
  }
}

void MqttTopic::OnSendFailure5(void *context, MQTTAsync_failureData5 *response) {
  auto *topic = reinterpret_cast<MqttTopic *>(context);
  if (topic != nullptr ) {
    topic->SetAllMetricsInvalid();

    auto& listen = topic->parent_.Listen();
    if (listen.IsActive() && response != nullptr) {
      listen.ListenText("Publish Failure. Topic:: %s, Error: %s",
                        topic->Topic().c_str(),
                        MQTTAsync_strerror(response->code));
    }
  }
}

}