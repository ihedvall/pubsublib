/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include <util/timestamp.h>
#include "sparkplug_b_c_sharp.pb.h"
#include "pubsub/ipayload.h"
#include "payloadhelper.h"
using namespace util::time;
using namespace org::eclipse::tahu::protobuf;

namespace pub_sub::test {

TEST(Sparkplug, RawPayload) { // NOLINT
  DataType type = DataType::DateTime;

  Payload node_birth;

  node_birth.set_timestamp(TimeStampToNs() / 1'000'000);
  node_birth.set_seq(0);

  auto* reboot = node_birth.add_metrics();
  reboot->set_name("Node Control/Reboot");
  reboot->set_alias(1);
  reboot->set_datatype(DataType::Boolean);
  reboot->set_boolean_value(false);

  auto* scan_rate = node_birth.add_metrics();
  scan_rate->set_name("Node Control/Scan Rate");
  scan_rate->set_alias(2);
  scan_rate->set_datatype(DataType::Double);
  scan_rate->set_double_value(1.0);

  auto* props = new Payload_PropertySet;
  for (size_t index = 0; index < 10; ++index) {
      std::ostringstream temp;
      temp << "Prop" << index;
      props->add_keys(temp.str());
      auto* prop_values = props->add_values();
      prop_values->set_type(DataType::String);
      prop_values->set_string_value("Hz");
  }
  scan_rate->set_allocated_properties(props);

  std::cout << "Payload: " << node_birth.DebugString() << std::endl;

}

TEST(Sparkplug, MetricValue) {
  IValue metric;
  for (int index = INT8_MIN; index <= INT8_MAX; ++index ) {
    const auto orig = static_cast<int8_t>(index);
    metric.Value(orig);
    const auto dest = metric.Value<int8_t>();
    // std::cout << metric.Value<std::string>() << "/" << static_cast<int>(dest) << std::endl;
    EXPECT_EQ(orig, dest);
  }

  for (int index = INT8_MIN; index <= INT8_MAX; ++index ) {
    const auto orig = static_cast<int64_t>(index);
    metric.Value(orig);
    const auto dest = metric.Value<int64_t>();
    EXPECT_EQ(orig, dest);
  }

  for (int index = 0; index <= UINT8_MAX; ++index ) {
    const auto orig = static_cast<uint8_t>(index);
    metric.Value(orig);
    const auto dest = metric.Value<uint8_t>();
    // std::cout << metric.Value<std::string>() << "/" << static_cast<uint64_t>(dest) << std::endl;
    EXPECT_EQ(orig, dest);
  }

  for (int index = 0; index <= UINT8_MAX; ++index ) {
    const auto orig = static_cast<uint64_t>(index);
    metric.Value(orig);
    const auto dest = metric.Value<uint64_t>();
    EXPECT_EQ(orig, dest);
  }
  {
    const auto orig = UINT64_MAX;
    metric.Value(orig);
    const auto dest = metric.Value<uint64_t>();
    EXPECT_EQ(orig, dest);
    EXPECT_EQ(dest, UINT64_MAX);
  }

  for (float index = -11.23F; index < 11.23F; index += 0.1F ) { // NOLINT
    metric.Value(index);
    const auto dest = metric.Value<float>();
    EXPECT_EQ(index, dest);
  }

  for (double index = -11.23; index < 11.23; index += 0.1 ) { // NOLINT
    metric.Value(index);
    const auto dest = metric.Value<double>();
    EXPECT_EQ(index, dest);
  }

  {
    metric.Value(true);
    const auto dest = metric.Value<bool>();
    EXPECT_TRUE(dest);
  }

  {
    metric.Value(false);
    const auto dest = metric.Value<bool>();
    EXPECT_FALSE(dest);
  }

  {
    const std::string orig = "Hello Test";
    metric.Value(orig);
    const auto dest = metric.Value<std::string>();
    EXPECT_EQ(dest, orig);
  }

  {
    const std::string orig = "Hello Test";
    metric.Value(orig.c_str());
    const auto dest = metric.Value<std::string>();
    EXPECT_EQ(dest, orig);
  }
}

TEST(Sparkplug, TestMetric) {
  IValue orig;
  const auto ms_now = util::time::TimeStampToNs() / 1'000'000;
  const int8_t value = -11;

  orig.Name("Metric 1");
  orig.Alias(11);
  orig.Timestamp(ms_now);
  orig.Type(ValueType::Int8);
  orig.Value(value);

  Property prop1 = {"Scan Rate", ValueType::String, false, "Hz"};
  orig.AddProperty(prop1);

  Property prop2 = {"Read-Only", ValueType::Boolean, false,"true"};
  orig.AddProperty(prop2);

  Property prop3 = {"Description", ValueType::String, false, "Descriptive text"};
  orig.AddProperty(prop3);

  std::vector<uint8_t> body;
  orig.GetBody(body);

  Payload_Metric temp;
  temp.ParseFromArray(body.data(), static_cast<int>(body.size()));
  std::cout << temp.DebugString() << std::endl;

  IValue dest;
  PayloadHelper::ProtobufToMetric(temp, dest);

  std::cout << dest.DebugString() << std::endl;
}

} // end namespace