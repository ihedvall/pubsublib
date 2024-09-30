/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <memory>
#include <string_view>
#include "sparkplugdevice.h"
#include "sparkplugnode.h"
#include "sparkplugtopic.h"
#include "util/logstream.h"

namespace {

constexpr std::string_view kNamespace = "spvBv1.0";
constexpr std::string_view kSequenceNumber = "seq";

constexpr std::string_view kReboot = "Device Control/Reboot";
constexpr std::string_view kRebirth = "Device Control/Rebirth";
constexpr std::string_view kScanRate = "Device Control/Scan Rate";

constexpr std::string_view kHardwareMake = "Properties/Hardware Make";
constexpr std::string_view kHardwareModel = "Properties/Hardware Model";
constexpr std::string_view kFirmware = "Properties/FW";
constexpr std::string_view kFirmwareVersion = "Properties/FW Version";
}
namespace pub_sub {

SparkplugDevice::SparkplugDevice(SparkplugNode &parent)
: IPubSubClient(),
  parent_(parent) {
  // Create the DBIRTH topic so it is possible to add metrics to it
  CreateDeviceDeathTopic();
  CreateDeviceBirthTopic();

}

bool SparkplugDevice::IsOnline() const {
  return device_state_ == DeviceState::Online ;
}

bool SparkplugDevice::IsOffline() const {
  return device_state_ == DeviceState::Offline;
}

ITopic *SparkplugDevice::AddMetric(const std::shared_ptr<Metric> &value) {
  // Add the metric to this device DBIRTH
  auto* topic = GetTopicByMessageType("DBIRTH");
  if (topic != nullptr) {
    auto& payload = topic->GetPayload();
    payload.AddMetric(value);
  }
  return topic;
}

ITopic *SparkplugDevice::CreateTopic() {
  // Note that parent to the topic is the node not this device.
  // The topic is however added to this device.
  auto topic = std::make_unique<SparkplugTopic>(parent_);
  std::scoped_lock list_lock(topic_mutex_);
  topic_list_.emplace_back(std::move(topic));
  return topic_list_.back().get();
}

bool SparkplugDevice::Start() {
  device_state_ = DeviceState::Idle;
  SetAllMetricsInvalid();
  return true;
}

bool SparkplugDevice::Stop() {
  SetAllMetricsInvalid();
  if (device_state_ == DeviceState::Online && parent_.IsOnline()) {
    PublishDeviceDeath();
  }
  return true;
}

bool SparkplugDevice::IsConnected() const {
  return parent_.IsConnected();
}

void SparkplugDevice::CreateDeviceDeathTopic() {
  auto* listen = parent_.Listen();
  auto* topic = GetTopicByMessageType("DDEATH");
  if (topic != nullptr) {
    // Need to delete the current topic and create a new
    // in case of any configuration change.
    if (listen != nullptr && listen->IsActive()) {
      listen->ListenText("Deleting previous DDEATH message");
    }
    DeleteTopic(topic->Topic());
  }

  std::ostringstream topic_name;
  topic_name << "spvBv1.0/" << GroupId() << "/DDEATH/" << parent_.Name() << "/" << Name();

  topic = CreateTopic();
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to create DDEATH topic. Node/Device: " << parent_.Name() << "/" << Name();
    return;
  }
  topic->Topic(topic_name.str());
  topic->Namespace("spvBv1.0");
  topic->GroupId(GroupId());
  topic->MessageType("DDEATH");
  topic->NodeId(parent_.Name());
  topic->DeviceId(Name());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos0);
  topic->Retained(false);

  auto& payload = topic->GetPayload();
  payload.Timestamp(SparkplugHelper::NowMs(), true);
  // Todo: Fix sequence number
}

void SparkplugDevice::CreateDeviceBirthTopic() {
  auto* listen = parent_.Listen();
  auto* topic = GetTopicByMessageType("DBIRTH");
  if (topic != nullptr) {
    // Need to delete the current topic and create a new
    // in case of any configuration change.
    if (listen != nullptr && listen->IsActive()) {
      listen->ListenText("Deleting previous DBIRTH message. Node/Device: %s/%s",
                         parent_.Name().c_str(), Name().c_str());
    }
    DeleteTopic(topic->Topic());
  }

  std::ostringstream topic_name;
  topic_name << kNamespace << "/" << GroupId() << "/DBIRTH/" << parent_.Name() << "/" << Name();

  topic = CreateTopic();
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to create the DBIRTH topic.";
    return;
  }

  topic->Topic(topic_name.str());
  topic->Namespace(kNamespace.data());
  topic->GroupId(GroupId());
  topic->MessageType("DBIRTH");
  topic->NodeId(parent_.Name());
  topic->DeviceId(Name());
  topic->Publish(true);
  topic->Qos(QualityOfService::Qos0);
  topic->Retained(false);

  auto& payload = topic->GetPayload();

  auto reboot = payload.CreateMetric(kReboot.data());
  if (reboot) {
    reboot->Type(MetricType::Boolean);
    reboot->Value(false);
  }

  auto rebirth = payload.CreateMetric(kRebirth.data());
  if (rebirth) {
    rebirth->Type(MetricType::Boolean);
    rebirth->Value(false);
  }

  auto scan_rate = payload.CreateMetric(kScanRate.data());
  if (scan_rate) {
    scan_rate->Type(MetricType::Int64);
    scan_rate->Value(false);
    scan_rate->Unit("ms");
  }

  auto hardware_make = !HardwareMake().empty() ?  payload.CreateMetric(kHardwareMake.data()) : nullptr;
  if (hardware_make) {
    hardware_make->Type(MetricType::String);
    hardware_make->Value(HardwareMake());
  }

  auto hardware_model = !HardwareModel().empty() ?  payload.CreateMetric(kHardwareModel.data()) : nullptr;
  if (hardware_model) {
    hardware_model->Type(MetricType::String);
    hardware_model->Value(HardwareModel());
  }

  auto firmware = !OperatingSystem().empty() ?  payload.CreateMetric(kFirmware.data()) : nullptr;
  if (firmware) {
    firmware->Type(MetricType::String);
    firmware->Value(OperatingSystem());
  }

  auto fw_version = !OsVersion().empty() ?  payload.CreateMetric(kFirmwareVersion.data()) : nullptr;
  if (fw_version) {
    fw_version->Type(MetricType::String);
    fw_version->Value(OsVersion());
  }

  payload.Timestamp(SparkplugHelper::NowMs(), true);
  // Todo: fix sequence number
}

void SparkplugDevice::Poll() {
  switch (device_state_) {
    case DeviceState::Online:
      if (!InService() || !parent_.IsOnline() || !parent_.InService()) {
        PublishDeviceDeath();
        SetAllMetricsInvalid();
        device_state_ = DeviceState::Offline;
      } else {
          // Todo: Send DDATA if needed
          // Todo: Handle any commands to do
      }
      break;

    case DeviceState::Offline:
      if (InService() || parent_.IsOnline() ) {
        PublishDeviceBirth();
        device_state_ = DeviceState::Online;
      }
      break;

    default:
      SetAllMetricsInvalid();
      device_state_ = DeviceState::Offline;
      break;
  }
}

void SparkplugDevice::SetAllMetricsInvalid() {
  for (auto& topic : topic_list_) {
    if (!topic || topic->MessageType() != "DBIRTH") {
      continue;
    }
    topic->SetAllMetricsInvalid();
  }

}

void SparkplugDevice::PublishDeviceBirth() {

  auto* birth_topic = GetTopicByMessageType("DBIRTH");
  if (birth_topic != nullptr) {
    std::ostringstream topic_name;
    topic_name << "spvBv1.0/" << GroupId() << "/DBIRTH/" << parent_.Name() << "/" << Name();
    birth_topic->Topic(topic_name.str());

    auto& payload = birth_topic->GetPayload();
    payload.Timestamp(SparkplugHelper::NowMs());
    // Todo: Handle sequence number
    if (parent_.IsConnected()) {
      birth_topic->DoPublish();
    }
  } else {
    LOG_ERROR() << "No DBIRTH message defined. Internal error";
  }
}

void SparkplugDevice::PublishDeviceDeath() {

  auto* death_topic = GetTopicByMessageType("DDEATH");
  if (death_topic != nullptr) {
    std::ostringstream topic_name;
    topic_name << "spvBv1.0/" << GroupId() << "/DDEATH/" << parent_.Name() << "/" << Name();
    death_topic->Topic(topic_name.str());
    auto& payload = death_topic->GetPayload();
    payload.Timestamp(SparkplugHelper::NowMs());
    // Todo: Handle sequence number
    if (parent_.IsConnected()) {
      death_topic->DoPublish();
    }
  } else {
    LOG_ERROR() << "No DDEATH message defined. Internal error";
  }
}

} // pub_sub