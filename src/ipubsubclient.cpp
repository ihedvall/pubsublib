/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsub/ipubsubclient.h"

#include <algorithm>
#include <filesystem>

#include <util/ihwinfo.h>
#include <util/ixmlfile.h>
#include <util/stringutil.h>
#include <util/logstream.h>

#include "sparkplughost.h"

using namespace util::string;
using namespace util::hw_info;
using namespace util::xml;
using namespace util::log;
using namespace std::filesystem;

namespace {
using namespace pub_sub;

std::string_view TransportToString(TransportLayer transport) {
  switch (transport) {
    case TransportLayer::MqttTcpTls:
      return "MqttTcpTls";

    case TransportLayer::MqttWebSocket:
      return "MqttWebSocket";

    case TransportLayer::MqttWebSocketTls:
      return "MqttWebSocketTls";

    case TransportLayer::MqttTcp:
    default:
      break;
  }
  return "MqttTcp";
}

TransportLayer TransportFromString(const std::string& transport) {
  if ( IEquals(transport, "MqttTcpTls") ) {
    return TransportLayer::MqttTcpTls;
  }
  if ( IEquals(transport, "MqttWebSocket") ) {
    return TransportLayer::MqttWebSocket;
  }
  if ( IEquals(transport, "MqttWebSocketTls") ) {
    return TransportLayer::MqttWebSocketTls;
  }
  return TransportLayer::MqttTcp;
}

std::string_view VersionToString(ProtocolVersion version)  {
  switch (version) {
    case ProtocolVersion::Mqtt31:
      return "MQTT 3.1";

    case ProtocolVersion::Mqtt5:
      return "MQTT 5.0";

    default:
      break;
  }
  return "MQTT 3.1.1";
}

ProtocolVersion VersionFromString(const std::string& version)  {
  if (IEquals(version, "MQTT 3.1")) {
    return ProtocolVersion::Mqtt31;
  }
  if (IEquals(version, "MQTT 5.0")) {
    return ProtocolVersion::Mqtt5;
  }
  return ProtocolVersion::Mqtt311;
}

}

namespace pub_sub {

IPubSubClient::IPubSubClient()
: hardware_make_(IHwInfo::CpuVendor()),
  hardware_model_(IHwInfo::CpuModel()),
  operating_system_(IHwInfo::OsName()),
  os_version_(IHwInfo::OsKernel()) {

}

ITopic *IPubSubClient::GetTopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex_);

  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && topic_name == topic->Topic();
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetITopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex_);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();
}

ITopic *IPubSubClient::GetTopicByMessageType(const std::string &message_type) {
  std::scoped_lock list_lock(topic_mutex_);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(message_type,topic->MessageType());
  });
  return itr == topic_list_.end() ? nullptr : itr->get();

}
void IPubSubClient::DeleteTopic(const std::string &topic_name) {
  std::scoped_lock list_lock(topic_mutex_);
  auto itr = std::ranges::find_if(topic_list_,[&] (const auto& topic) {
    return topic && IEquals(topic_name,topic->Topic());
  });
  if (itr != topic_list_.end()) {
    topic_list_.erase(itr);
  }
}

void IPubSubClient::AddSubscription(std::string topic_name) {
  const bool exist = std::any_of(subscription_list_.cbegin(), subscription_list_.cend(),
                                 [&] (const std::string& topic)->bool {
                                   return topic_name == topic;
                                 });
  if (!exist) {
    subscription_list_.emplace_back(std::move(topic_name));
  }
}

void IPubSubClient::AddSubscriptionFront(std::string topic_name) {
  const bool exist = std::any_of(subscription_list_.cbegin(), subscription_list_.cend(),
                                 [&] (const std::string& topic)->bool {
                                   return topic_name == topic;
                                 });
  if (!exist) {
    subscription_list_.emplace_front(std::move(topic_name));
  }
}

void IPubSubClient::DeleteSubscription(const std::string &topic_name) {
  auto itr = std::find_if(subscription_list_.begin(), subscription_list_.end(),
                          [&] (const std::string& topic)->bool {
                            return topic_name == topic;
                          });
  if (itr != subscription_list_.end())  {
    subscription_list_.erase(itr);
  }
}

const std::list<std::string> &IPubSubClient::Subscriptions() const {
  return subscription_list_;
}

void IPubSubClient::ScanRate(int64_t scan_rate) {
  scan_rate_ = scan_rate;
}

int64_t IPubSubClient::ScanRate() const {
  return scan_rate_;
}

IPubSubClient *IPubSubClient::CreateDevice(const std::string &) {
  return nullptr;
}

void IPubSubClient::DeleteDevice(const std::string &) {
}

IPubSubClient *IPubSubClient::GetDevice(const std::string &) {
  return nullptr;
}

const IPubSubClient *IPubSubClient::GetDevice(const std::string &) const {
  return nullptr;
}

std::string IPubSubClient::VersionAsString() const {
  switch (Version()) {
    case ProtocolVersion::Mqtt31:
      return "MQTT 3.1";

    case ProtocolVersion::Mqtt5:
      return "MQTT 5.0";

    default:
      break;
  }
  return "MQTT 3.1.1";
}


void IPubSubClient::PublishTopics() {
  std::scoped_lock lock(topic_mutex_);
  for (auto& topic : topic_list_) {
    if (!topic || !topic->Publish() || !topic->IsUpdated()) {
      continue;
    }
    if (topic->IsUpdated()) {
      topic->ResetUpdated();
      topic->DoPublish();
    }
  }
}

void IPubSubClient::WriteGeneralXml(IXmlNode &general) const {
  general.SetProperty("Name", name_);
  general.SetProperty("GroupId", group_);
  general.SetProperty("Transport", TransportToString(transport_));
  general.SetProperty("Broker", broker_);
  general.SetProperty("Port", port_);
  general.SetProperty("ProtocolVersion", VersionToString(version_));
  general.SetProperty("HardwareMake", hardware_make_);
  general.SetProperty("HardwareModel", hardware_model_);
  general.SetProperty("OperatingSystem", operating_system_);
  general.SetProperty("OsVersion", os_version_);
  general.SetProperty("ScanRate", scan_rate_);
  general.SetProperty("WaitOnHostOnline", wait_on_host_online_);
  general.SetProperty("Username", username_);
  general.SetProperty("Password", password_);
}

void IPubSubClient::ReadGeneralXml(const IXmlNode &general) {
  if (general.ExistProperty("Name")) {
    Name(general.Property<std::string>("Name"));
  }
  if (general.ExistProperty("GroupId")) {
    GroupId(general.Property<std::string>("GroupId"));
  }
  if (general.ExistProperty("Transport")) {
    const auto transport = general.Property<std::string>("Transport");
    Transport(TransportFromString(transport));
  }
  if (general.ExistProperty("Broker")) {
    Broker(general.Property<std::string>("Broker"));
  }
  if (general.ExistProperty("Port")) {
    Port(general.Property<uint16_t>("Port"));
  }
  if (general.ExistProperty("ProtocolVersion")) {
    const auto version = general.Property<std::string>("ProtocolVersion");
    Version(VersionFromString(version));
  }
  if (general.ExistProperty("HardwareMake")) {
    HardwareMake(general.Property<std::string>("HardwareMake"));
  }
  if (general.ExistProperty("HardwareModel")) {
    HardwareModel(general.Property<std::string>("HardwareModel"));
  }
  if (general.ExistProperty("OperatingSystem")) {
    OperatingSystem(general.Property<std::string>("OperatingSystem"));
  }
  if (general.ExistProperty("OsVersion")) {
    OsVersion(general.Property<std::string>("OsVersion"));
  }
  if (general.ExistProperty("ScanRate")) {
    ScanRate(general.Property<int64_t>("ScanRate"));
  }
  if (general.ExistProperty("WaitOnHostOnline")) {
    WaitOnHostOnline(general.Property<bool>("WaitOnHostOnline"));
  }
  if (general.ExistProperty("Username")) {
    username_ = general.Property<std::string>("Username");
  }
  if (general.ExistProperty("Password")) {
    password_ = general.Property<std::string>("Password");
  }
}

void IPubSubClient::WriteSslXml(IXmlNode &ssl_node) const {
  ssl_node.SetProperty("TrustStore",trust_store_);
  ssl_node.SetProperty("KeyStore", key_store_);
  ssl_node.SetProperty("PrivateKey", private_key_);
  ssl_node.SetProperty("PrivateKeyPassword",private_key_password_);
  ssl_node.SetProperty("EnabledCipherSuites", enabled_cipher_suites_);
  ssl_node.SetProperty("EnableCertAuth", enable_cert_auth_);
  ssl_node.SetProperty("SslVersion", ssl_version_);
  ssl_node.SetProperty("CaPath", ca_path_);
  ssl_node.SetProperty("DisableDefaultTrustStore", disable_default_trust_store_);
}

void IPubSubClient::ReadSslXml(const IXmlNode &ssl_node) {
  trust_store_ = ssl_node.Property<std::string>("TrustStore");
  key_store_ = ssl_node.Property<std::string>("KeyStore");
  private_key_ = ssl_node.Property<std::string>("PrivateKey" );
  private_key_password_ = ssl_node.Property<std::string>("PrivateKeyPassword");
  enabled_cipher_suites_ = ssl_node.Property<std::string>("EnabledCipherSuites" );
  enable_cert_auth_ = ssl_node.Property<bool>("EnableCertAuth", false );
  ssl_version_ = ssl_node.Property<int>("SslVersion", 0);
  ca_path_ = ssl_node.Property<std::string>("CaPath");
  disable_default_trust_store_ = ssl_node.Property<bool>("DisableDefaultTrustStore", false);
}


bool IPubSubClient::WriteConfiguration() {
  // Try to generate the config file path
  try {
    const path filename = config_file_;
    const path file_path = filename.parent_path();
    create_directories(file_path);
    auto xml_file = CreateXmlFile();
    if (!xml_file) {
      LOG_ERROR() << "Failed to create the XML file object";
      return false;
    }
    xml_file->FileName(config_file_);
    auto& root_node = xml_file->RootName("PubSubClient");
    auto& general = root_node.AddNode("General");
    WriteGeneralXml(general);

    auto& ssl = root_node.AddNode("SslOptions");
    WriteSslXml(ssl);

  } catch (const std::exception& err) {
    LOG_ERROR() << "Failed to create config file path. Error: " << err.what();
    return false;
  }
  return true;
}

bool IPubSubClient::ReadConfiguration() {
  try {
    const path filename = config_file_;
    if (!exists(filename)) {
      LOG_ERROR() << "The config file doesn't exist. File: " << config_file_;
      return false;
    }
    auto xml_file = CreateXmlFile();
    if (!xml_file) {
      throw std::runtime_error("Failed to create the XML object.");
    }
    xml_file->FileName(config_file_);
    const bool  parse = xml_file->ParseFile();
    if (!parse) {
      throw std::runtime_error("Failed to parse the XML file.");
    }
    const auto* root_node = xml_file->RootNode();
    if (root_node == nullptr) {
      throw std::runtime_error("No root node in XML file.");
    }
    const auto* general = root_node->GetNode("General");
    if (general != nullptr) {
      ReadGeneralXml(*general);
    }

    const auto* ssl = root_node->GetNode("SslOptions");
    if (ssl != nullptr) {
      ReadSslXml(*ssl);
    }

  } catch (const std::exception& err) {
    LOG_ERROR() << "Failed to read config file. Error: " << err.what();
    return false;
  }
  return true;
}

} // end namespace mqtt