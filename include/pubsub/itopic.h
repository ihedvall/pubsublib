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
   [[nodiscard]] const std::string& Topic() const;

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

  [[nodiscard]] bool IsUpdated() const;
  void ResetUpdated() const;

  [[nodiscard]] Payload& GetPayload() { return payload_; }
  [[nodiscard]] const Payload& GetPayload() const { return payload_; }

  virtual void DoPublish() = 0;

  [[nodiscard]] bool IsWildcard() const;

  std::shared_ptr<Metric> CreateMetric(const std::string& name);
  std::shared_ptr<Metric> GetMetric(const std::string& name) const;

  void SetAllMetricsInvalid();

 protected:
  mutable std::recursive_mutex topic_mutex_;

  [[nodiscard]] bool IsText() const {
    return content_type_.empty() || content_type_.find("text") != std::string::npos;
  }

  [[nodiscard]] bool IsJson() const {
    return content_type_.find("json") != std::string::npos;
  }

  [[nodiscard]] bool IsProtobuf() const {
    return content_type_.find("protobuf") != std::string::npos;
  }
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

} // end namespace pub_sub
