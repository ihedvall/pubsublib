/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "mqtttopic.h"
#include "mqttclient.h"

namespace pub_sub {
MqttTopic::MqttTopic(MqttClient &parent)
: parent_(parent) {
}

void MqttTopic::DoPublish() {
  lrv_ = Payload<std::vector<uint8_t>>();
  auto& listen = parent_.Listen();
  if (listen.IsActive() && listen.LogLevel() != 2) {
    const auto payload = Payload<std::string>();
    listen.ListenText("Publish: %s:%s", Topic().c_str(), payload.c_str());
  }

  MQTTAsync_message  message = MQTTAsync_message_initializer;
  message.payload = lrv_.data();
  message.payloadlen = static_cast<int>(lrv_.size());
  message.qos = static_cast<int>(Qos());
  message.retained = Retained() ? 1 : 0;

  MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
  options.onSuccess = OnSend;
  options.onFailure = OnSendFailure;
  options.context = this;

  const auto send = MQTTAsync_sendMessage(parent_.Handle(), Topic().c_str(), &message, &options );
  if (send != MQTTASYNC_SUCCESS) {
    message_sent_ = true;
    message_failed_ = true;
  }
}

void MqttTopic::DoSubscribe() {
  lrv_ = Payload<std::vector<uint8_t>>();
  auto& listen = parent_.Listen();
  if (listen.IsActive() && listen.LogLevel() != 2) {
    const auto payload = Payload<std::string>();
    listen.ListenText("Subscribe: %s", Topic().c_str());
  }

  MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
  options.onSuccess = OnSubscribe;
  options.onFailure = OnSubscribeFailure;
  options.context = this;

  const auto subscribe = MQTTAsync_subscribe(parent_.Handle(), Topic().c_str(), static_cast<int>(Qos()), &options );
  if (subscribe != MQTTASYNC_SUCCESS) {
    message_sent_ = true;
    message_failed_ = true;
  }
}

void MqttTopic::OnSendFailure(void *context, MQTTAsync_failureData *response) {
  auto *topic = reinterpret_cast<MqttTopic *>(context);
  if (topic != nullptr) {
    topic->message_sent_ = true;
    topic->message_failed_ = true;

    auto& value = topic->Value();
    if (value) {
      value->IsNull(true);
    }
    auto& listen = topic->parent_.Listen();
    if (listen.IsActive() && response != nullptr) {
      const auto payload = topic->Payload<std::string>();
      listen.ListenText("Publish Failure: %s:%s, Error: %s",
                        topic->Topic().c_str(), payload.c_str(),
                        MQTTAsync_strerror(response->code));
    }
  }
}

void MqttTopic::OnSend(void *context, MQTTAsync_successData *response) {
  auto *topic = reinterpret_cast<MqttTopic *>(context);
  if (topic != nullptr) {
    topic->message_sent_ = true;
    topic->message_failed_ = false;

    auto& value = topic->Value();
    if (value) {
      value->IsNull(false);
    }
    auto& listen = topic->parent_.Listen();
    if (listen.IsActive() && listen.LogLevel() == 3) {
      const auto payload = topic->Payload<std::string>();
      listen.ListenText("Publish Sent: %s:%s",
                        topic->Topic().c_str(), payload.c_str() );
    }
  }
}

void MqttTopic::OnSubscribeFailure(void *context, MQTTAsync_failureData *response) {
  auto *topic = reinterpret_cast<MqttTopic *>(context);
  if (topic != nullptr) {
    topic->message_sent_ = true;
    topic->message_failed_ = true;
    auto& value = topic->Value();
    if (value) {
      value->IsNull(true);
    }
    auto& listen = topic->parent_.Listen();
    if (listen.IsActive() && response != nullptr) {
      listen.ListenText("Subscribe Failure: %s, Error: %s",
                        topic->Topic().c_str(), MQTTAsync_strerror(response->code));
    }
  }
}

void MqttTopic::OnSubscribe(void *context, MQTTAsync_successData *response) {
  auto *topic = reinterpret_cast<MqttTopic *>(context);
  if (topic != nullptr) {
    topic->message_sent_ = true;
    topic->message_failed_ = false;
    auto& value = topic->Value();
    if (value) {
      value->IsNull(false);
    }
    auto& listen = topic->parent_.Listen();
    if (listen.IsActive() && listen.LogLevel() == 3) {
      listen.ListenText("Subscribe Sent: %s", topic->Topic().c_str());
    }
  }
}

void MqttTopic::ParsePayloadData() {
  // Todo: Clean up the MQTT client design
}

}