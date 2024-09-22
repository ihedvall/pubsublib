/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <mutex>
#include <memory>
#include "pubsub/payload.h"
#include "pubsub/metric.h"

namespace pub_sub {

enum class QualityOfService : int {
  Qos0 = 0, ///< Fire and forget. The message may not be delivered.
  Qos1 = 1, ///< At least once. The message will be delivered.
  Qos2 = 2, ///< Once and once only. The message will be delivered.
};

class ITopic {
 public:
  ITopic() = default;
  virtual ~ITopic() = default;

  void Topic(const std::string& topic);
   [[nodiscard]] virtual const std::string& Topic() const;

  void Namespace(const std::string& name_space) {
    name_space_ = name_space;
  }
  [[nodiscard]] const std::string& Namespace() const {
    return name_space_;
  }

  void GroupId(const std::string& group_id) {
    group_id_ = group_id;
  }
  [[nodiscard]] const std::string& GroupId() const {
    return group_id_;
  }

  void MessageType(const std::string& message_type) {
    message_type_ = message_type;
  }
  [[nodiscard]] const std::string& MessageType() const {
    return message_type_;
  }

  void NodeId(const std::string& node_id) {
    node_id_ = node_id;
  }
  [[nodiscard]] const std::string& NodeId() const {
    return node_id_;
  }

  void DeviceId(const std::string& device_id) {
    device_id_ = device_id;
  }
  [[nodiscard]] const std::string& DeviceId() const {
    return device_id_;
  }

  void ContentType(const std::string& mime_type) {
    content_type_ = mime_type;
  }
  [[nodiscard]] const std::string& ContentType() const {
    return content_type_;
  }
  [[nodiscard]] bool IsJson() const {
    return content_type_.find("json") != std::string::npos;
  }
  [[nodiscard]] bool IsProtobuf() const {
    return content_type_.find("protobuf") != std::string::npos;
  }

  void Publish(bool publish) {
    publish_ = publish;
  }
  [[nodiscard]] bool Publish() const {
    return publish_;
  }

  void Qos(QualityOfService qos) {
    qos_ =  qos;
  }
  [[nodiscard]] QualityOfService Qos() const {
    return qos_;
  }

  void Retained(bool retained) {
    retained_ =  retained;
  }
  [[nodiscard]] bool Retained() const {
    return retained_;
  }

  [[nodiscard]] bool Updated() const;

  template <typename T>
  void PayloadBody(const T& payload);

  template<typename T>
  [[nodiscard]] T PayloadBody() const;

  Payload& GetPayload() {
    return payload_;
  }
  virtual void DoPublish() = 0;
  virtual void DoSubscribe() = 0;

  [[nodiscard]] bool IsWildcard() const;

  void Value(const std::shared_ptr<Metric>& value) {
    value_ = value;
  }
  [[nodiscard]] std::shared_ptr<Metric>& Value() {
    return value_;
  }

  std::shared_ptr<Metric> CreateMetric(const std::string& name);
  std::shared_ptr<Metric> GetMetric(const std::string& name) const;

  virtual void ParsePayloadData() = 0;

  void SetAllMetricsInvalid();

 protected:
  mutable std::recursive_mutex topic_mutex_;
  bool       updated_ = false;
  uint64_t   update_counter_ = 0;
  std::shared_ptr<Metric> value_; ///< Reference to a user value object.

  virtual void UpdatePayload(const std::vector<uint8_t>& payload);

 private:
  std::string content_type_;    ///< MIME type of data (MQTT 5)

  mutable std::string topic_;   ///< MQTT topic name. If empty '<namespace>/<group_id>/<message_type>/<node_id>/<device_id>'
  std::string name_space_; ///< Topic namespace
  std::string group_id_;
  std::string message_type_;
  std::string node_id_;
  std::string device_id_;

  Payload payload_; ///< MQTT topic data.

  bool publish_ = false;
  QualityOfService qos_ = QualityOfService::Qos0;
  bool retained_ = false;

  void AssignLevelName(size_t level, const std::string& name);
};

template<typename T>
void ITopic::PayloadBody(const T& payload) {
  if (content_type_.empty()) {
    content_type_ = "text/plain";
  }
  std::ostringstream text;
  text << payload;
  payload_.StringToBody(text.str());
}


template<>
void ITopic::PayloadBody(const std::vector<uint8_t>& payload);

template<>
void ITopic::PayloadBody(const bool& payload);

template<>
void ITopic::PayloadBody(const float& payload);

template<>
void ITopic::PayloadBody(const double& payload);

template<typename T>
T ITopic::PayloadBody() const {
  std::lock_guard lock(topic_mutex_);
  T value = {};
  try {
    const auto& body = payload_.Body();
    std::string temp(body.size(), '\0');
    memcpy_s(temp.data(), temp.size(), body.data(), body.size());
    std::istringstream str(temp);
    str >> value;
  } catch (const std::exception& ) {
  }
  return value;
}

template<>
std::vector<uint8_t> ITopic::PayloadBody() const;

template<>
bool ITopic::PayloadBody() const;

} // end namespace util::mqtt
