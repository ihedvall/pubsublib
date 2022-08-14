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

  void Topic(const std::string& topic) {
    topic_ = topic;
  }
  [[nodiscard]] const std::string& Topic() const {
    return topic_;
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

  [[nodiscard]] bool Updated() const;

  template <typename T>
  void Payload(const T& payload);

  template<typename T>
  [[nodiscard]] T Payload() const;

  virtual void DoPublish() = 0;
  virtual void DoSubscribe() = 0;

  [[nodiscard]] bool IsWildcard() const;
 protected:
  mutable std::recursive_mutex topic_mutex_;
  bool       updated_ = false;
  uint64_t   update_counter_ = 0;

  virtual void UpdatePayload(const std::vector<uint8_t>& payload);

 private:
  std::string content_type_;    ///< MIME type of data (MQTT 5)
  std::string topic_;   ///< MQTT topic name.
  std::vector<uint8_t> payload_; ///< MQTT topic data.
  uint64_t    timestamp_ = 0; ///< Nanoseconds since 1970


  bool publish_ = false;
  QualityOfService qos_ = QualityOfService::Qos0;
  bool retained_ = false;
};

template<typename T>
void ITopic::Payload(const T& payload) {
  if (content_type_.empty()) {
    content_type_ = "text/plain";
  }

  std::ostringstream text;
  text << payload;
  std::vector<uint8_t> temp(text.str().size(),0);
  memcpy_s(temp.data(), temp.size(), text.str().data(), text.str().size());

  Payload(temp);
}


template<>
void ITopic::Payload(const std::vector<uint8_t>& payload);

template<>
void ITopic::Payload(const bool& payload);

template<>
void ITopic::Payload(const float& payload);

template<>
void ITopic::Payload(const double& payload);

template<typename T>
T ITopic::Payload() const {
  std::lock_guard lock(topic_mutex_);
  T value = {};
  try {
    std::string temp(payload_.size(), '\0');
    memcpy_s(temp.data(), temp.size(), payload_.data(), payload_.size());
    std::istringstream str(temp);
    str >> value;
  } catch (const std::exception& ) {
  }
  return value;
}

template<>
std::vector<uint8_t> ITopic::Payload() const;

template<>
bool ITopic::Payload() const;

} // end namespace util::mqtt
