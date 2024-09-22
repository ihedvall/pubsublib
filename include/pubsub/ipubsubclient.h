/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <list>
#include <vector>
#include <functional>
#include <set>
#include <mutex>
#include <atomic>
#include "pubsub/itopic.h"

namespace pub_sub {

enum class TransportLayer: int {
  MqttTcp,
  MqttWebSocket,
  MqttTcpTls,
  MqttWebSocketTls,
};

enum class ProtocolVersion : int {
  Mqtt3 = 3,
  Mqtt311 = 4,
  Mqtt5 = 5
};

class SparkplugDevice;
class SparkplugNode;
class SparkplugHost;

/**
 * @brief The IPubSubClient class is an abstract interface for a publish-subscribe client.
 *
 * The class declares a generic interface to a public/subscriber top level objects as a MQTT client or
 * a SparkPlug B server.
 */
class IPubSubClient {
 public:
  using TopicList = std::vector<std::unique_ptr<ITopic>>;
  using ValueList = std::vector<std::shared_ptr<Metric>>;


  IPubSubClient();
  virtual ~IPubSubClient() = default;

  /** \brief Sets the Node Name/ID.
   *
   * Sets Node name or Node ID. Note that the node belongs to a group.
   * The group ID and the node ID must be unique.
   * @param name Node ID or Node Name.
   */
  void Name(const std::string& name) {
    name_ = name;
  }

  [[nodiscard]] const std::string& Name() const {
    return name_;
  }

  void GroupId(const std::string& group) { group_ = group; }
  [[nodiscard]] const std::string& GroupId() const { return group_; }


  void Transport(TransportLayer transport) {
    transport_ = transport;
  }

  [[nodiscard]] TransportLayer Transport() const {
    return transport_;
  }

  void Broker(const std::string& address) {
    broker_ = address;
  }
  [[nodiscard]] const std::string& Broker() const {
    return broker_;
  }

  void Port(uint16_t port) {
    port_ = port;
  }
  [[nodiscard]] uint16_t Port() const {
    return port_;
  }

  void Version(ProtocolVersion version) {
    version_ = version;
  }
  [[nodiscard]] ProtocolVersion Version() const {
    return version_;
  }

  void HardwareMake(const std::string& hardware_make) { hardware_make_ = hardware_make;}
  [[nodiscard]] const std::string& HardwareMake() const {return hardware_make_; }

  void HardwareModel(const std::string& hardware_model) { hardware_model_ = hardware_model;}
  [[nodiscard]] const std::string& HardwareModel() const {return hardware_model_; }

  void OperatingSystem(const std::string& operating_system) { operating_system_ = operating_system;}
  [[nodiscard]] const std::string& OperatingSystem() const {return operating_system_; }

  void OsVersion(const std::string& os_version) { os_version_ = os_version;}
  [[nodiscard]] const std::string& OsVersion() const {return os_version_; }

  virtual void ScanRate(int64_t scan_rate); ///< Scan rate in ms.
  [[nodiscard]] virtual int64_t ScanRate() const;

  void SparkplugVersion(const std::string& version) { sparkplug_version_ = version; }
  [[nodiscard]] const std::string& SparkplugVersion() const { return sparkplug_version_; }

  void MqttVersion(const std::string& version) { mqtt_version_ = version; }
  [[nodiscard]] const std::string& MqttVersion() const { return mqtt_version_; }

  void InService(bool in_service) { in_service_ = in_service; }
  [[nodiscard]] bool InService() const { return in_service_;}

  virtual bool IsOnline() const = 0;
  virtual bool IsOffline() const = 0;

  virtual ITopic* AddMetric(const std::shared_ptr<Metric>& value) = 0;
  virtual ITopic* CreateTopic() = 0;

  ITopic* GetTopic(const std::string& topic_name);
  ITopic* GetITopic(const std::string& topic_name);
  ITopic* GetTopicByMessageType(const std::string &message_type);
  void DeleteTopic(const std::string& topic_name);
  void ClearTopicList();

  virtual bool Start() = 0;
  virtual bool Stop() = 0;

  [[nodiscard]] virtual bool IsConnected() const = 0;

  void DefaultQualityOfService(QualityOfService quality) {
    default_qos_ = quality;
  }
  [[nodiscard]] QualityOfService DefaultQualityOfService() const {
    return default_qos_;
  }
  int GetUniqueToken();

  void AddSubscription(const std::string& topic_name);
  void DeleteSubscription(const std::string& topic_name);

  const std::list<std::string>& Subscriptions() const;

  [[nodiscard]] virtual IPubSubClient* CreateDevice(const std::string& device_name);
  virtual void DeleteDevice(const std::string& device_name);
  [[nodiscard]] virtual IPubSubClient* GetDevice(const std::string& device_name);
  [[nodiscard]] virtual const IPubSubClient* GetDevice(const std::string& device_name) const;

  [[nodiscard]] bool IsConnectionLost() const {return connection_lost_; }

 protected:

  ProtocolVersion version_ = ProtocolVersion::Mqtt311; ///< Using version 3.1.1 as default.
  TransportLayer transport_ = TransportLayer::MqttTcp; ///< Defines the underlying transport protocol and encryption.
  std::string broker_ = "127.0.0.1"; ///< Address to the MQTT server (broker).
  uint16_t port_ = 1883; ///< The MQTT broker server port.

  std::string name_;  ///< Name of the client.
  std::string group_; ///< Group ID (Sparkplug B).

  std::string hardware_make_; ///< Defines the hardware maker.
  std::string hardware_model_; ///< Defines the hardware model.
  std::string operating_system_; ///< Defines the operating system.
  std::string os_version_; ///< Defines the operating system version.
  std::string sparkplug_version_ = "3.0.0"; ///< Sparkplug version
  std::string mqtt_version_ ; ///< MQTT version set by the connect

  std::atomic<bool> reboot_ = false;
  std::atomic<bool> rebirth_ = false;
  std::atomic<bool> next_server_ = false;
  std::atomic<int64_t> scan_rate_ = 0; ///< Scan rate in ms.


  mutable std::recursive_mutex topic_mutex; ///< Thread protection of the topic list
  TopicList topic_list_; ///< List of topics.
  std::list<std::string> subscription_list_;

  void ResetConnectionLost() { connection_lost_ = false; }
  void SetConnectionLost() { connection_lost_ = true; }
  void AddSubscriptionFront(const std::string& topic_name);
private:

  QualityOfService default_qos_ = QualityOfService::Qos1;
  std::atomic<int> unique_token = 1;
  std::atomic<bool> in_service_ = true; ///< Sets the client online of offline
  /** \brief Connection lost or connection/disconnection fail
   *
   * The boolean is used at start and stop of connection and also to detect
   * any loss of connections during run-time.
   */
  std::atomic<bool> connection_lost_ = false;


};




} // end namespace
