/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include <util/timestamp.h>
#include "sparkplug_b.pb.h"
#include "pubsub/payload.h"
#include "payloadhelper.h"
#include "sparkplughelper.h"
using namespace util::time;
// using namespace org::eclipse::tahu::protobuf;

namespace pub_sub::test {

TEST(IPayload, TestAny) {
  {
    constexpr int8_t test_value = -32;
    const uint32_t pb_input = static_cast<uint8_t>(test_value);
    const auto output = static_cast<int8_t>(pb_input);
    EXPECT_EQ(output, test_value);
    std::any any_value = output;
    EXPECT_EQ(std::any_cast<int8_t>(any_value), test_value);
  }
}

TEST(Sparkplug, RawPayload) { // NOLINT
  org::eclipse::tahu::protobuf::DataType type =
      org::eclipse::tahu::protobuf::DataType::DateTime;

  org::eclipse::tahu::protobuf::Payload node_birth;

  node_birth.set_timestamp(SparkplugHelper::NowMs());
  node_birth.set_seq(0);

  auto* reboot = node_birth.add_metrics();
  reboot->set_name("Node Control/Reboot");
  reboot->set_alias(1);
  reboot->set_datatype( org::eclipse::tahu::protobuf::DataType::Boolean);
  reboot->set_boolean_value(false);

  auto* scan_rate = node_birth.add_metrics();
  scan_rate->set_name("Node Control/Scan Rate");
  scan_rate->set_alias(2);
  scan_rate->set_datatype( org::eclipse::tahu::protobuf::DataType::Double);
  scan_rate->set_double_value(1.0);

  auto* props = new org::eclipse::tahu::protobuf:: Payload_PropertySet;
  for (size_t index = 0; index < 10; ++index) {
      std::ostringstream temp;
      temp << "Prop" << index;
      props->add_keys(temp.str());
      auto* prop_values = props->add_values();
      prop_values->set_type( org::eclipse::tahu::protobuf::DataType::String);
      prop_values->set_string_value("Hz");
  }
  scan_rate->set_allocated_properties(props);

  std::cout << "Payload: " << node_birth.DebugString() << std::endl;

}

TEST(IPayload, MetricValue) {
  Metric metric;
  for (int index = INT8_MIN; index <= INT8_MAX; ++index ) {
    const auto orig = static_cast<int8_t>(index);
    metric.Type(MetricType::Int8);
    metric.Value(orig);
    const auto dest = metric.Value<int8_t>();
    // std::cout << metric.Value<std::string>() << "/" << static_cast<int>(dest) << std::endl;
    EXPECT_EQ(orig, dest);
  }

  for (int index = INT8_MIN; index <= INT8_MAX; ++index ) {
    const auto orig = static_cast<int64_t>(index);
    metric.Type(MetricType::Int64);
    metric.Value(orig);
    const auto dest = metric.Value<int64_t>();
    EXPECT_EQ(orig, dest);
  }

  for (int index = 0; index <= UINT8_MAX; ++index ) {
    const auto orig = static_cast<uint8_t>(index);
    metric.Type(MetricType::UInt8);
    metric.Value(orig);
    const auto dest = metric.Value<uint8_t>();
    // std::cout << metric.Value<std::string>() << "/" << static_cast<uint64_t>(dest) << std::endl;
    EXPECT_EQ(orig, dest);
  }

  for (int index = 0; index <= UINT8_MAX; ++index ) {
    const auto orig = static_cast<uint64_t>(index);
    metric.Type(MetricType::UInt64);
    metric.Value(orig);
    const auto dest = metric.Value<uint64_t>();
    EXPECT_EQ(orig, dest);
  }
  {
    const auto orig = UINT64_MAX;
    metric.Type(MetricType::UInt64);
    metric.Value(orig);
    const auto dest = metric.Value<uint64_t>();
    EXPECT_EQ(orig, dest);
    EXPECT_EQ(dest, UINT64_MAX);
  }
  {
    for (float index = -11.23F; index < 11.23F; index += 0.1F) { // NOLINT
      metric.Type(MetricType::Float);
      metric.Value<float>(index);
      const auto dest = metric.Value<float>();
      EXPECT_FLOAT_EQ(index, dest);
    }
  }
  {
    for (double index = -11.23; index < 11.23; index += 0.1) { // NOLINT
      metric.Type(MetricType::Double);
      metric.Value(index);
      const auto dest = metric.Value<double>();
      EXPECT_DOUBLE_EQ(index, dest);
    }
  }
  {
    metric.Type(MetricType::Boolean);
    metric.Value(true);
    const auto dest = metric.Value<bool>();
    EXPECT_TRUE(dest);
  }

  {
    metric.Type(MetricType::Boolean);
    metric.Value(false);
    const auto dest = metric.Value<bool>();
    EXPECT_FALSE(dest);
  }

  {
    const std::string orig = "Hello Test";
    metric.Type(MetricType::String);
    metric.Value(orig);
    const auto dest = metric.Value<std::string>();
    EXPECT_EQ(dest, orig);
  }

  {
    const std::string orig = "Hello Test";
    metric.Type(MetricType::String);
    metric.Value(orig.c_str());
    const auto dest = metric.Value<std::string>();
    EXPECT_EQ(dest, orig);
  }
}

TEST(IPayload, TestMetric) {
  Payload payload;

  Metric orig;
  const auto ms_now = SparkplugHelper::NowMs();
  constexpr int8_t value = -11;

  orig.Name("Metric 1");
  orig.Alias(11);
  orig.Timestamp(ms_now);
  orig.Type(MetricType::Int8);
  orig.Value(value);
  orig.IsHistorical(true);
  orig.IsTransient(true);
  orig.IsNull(true);

  MetricProperty prop1;
  prop1.Key("Scan Rate");
  prop1.Type( MetricType::String);
  prop1.IsNull(false);
  prop1.Value("Hz");

  orig.AddProperty(prop1);

  MetricProperty prop2;
  prop2.Key("Read-Only");
  prop2.Type( MetricType::Boolean);
  prop2.IsNull(false);
  prop2.Value("true");
  orig.AddProperty(prop2);

  MetricProperty prop3;
  prop3.Key("Description");
  prop3.Type( MetricType::String);
  prop3.IsNull(false);
  prop3.Value("Descriptive text");
  orig.AddProperty(prop3);

  EXPECT_STREQ(orig.Name().c_str(), "Metric 1");
  EXPECT_EQ(orig.Alias(), 11);
  EXPECT_EQ(orig.Timestamp(), ms_now);
  EXPECT_EQ(orig.Type(), MetricType::Int8);
  EXPECT_EQ(orig.Value<int8_t>(), value);
  EXPECT_EQ(orig.IsHistorical(), true);
  EXPECT_EQ(orig.IsTransient(), true);
  EXPECT_EQ(orig.IsNull(), true);
  EXPECT_EQ(orig.Properties().size(), 3);

  std::vector<uint8_t> body;
  orig.GetBody(body);

  org::eclipse::tahu::protobuf::Payload_Metric temp;
  EXPECT_TRUE(temp.ParseFromArray(body.data(), static_cast<int>(body.size())));
  std::cout << temp.DebugString() << std::endl;

  Metric dest;
  PayloadHelper helper(payload);
  helper.CreateMetrics(true);
  helper.ParseMetric(temp, dest);

  EXPECT_EQ(orig.Name(), dest.Name());
  EXPECT_EQ(orig.Alias(), dest.Alias());
  EXPECT_EQ(orig.Timestamp(), dest.Timestamp());
  EXPECT_EQ(orig.Type(), dest.Type());
  EXPECT_EQ(orig.IsHistorical(), dest.IsHistorical());
  EXPECT_EQ(orig.IsTransient(), dest.IsTransient());
  EXPECT_EQ(orig.IsNull(), dest.IsNull());
  ASSERT_EQ(orig.Properties().size(), dest.Properties().size());
  auto orig_itr = orig.Properties().cbegin();
  auto dest_itr = dest.Properties().cbegin();
  for (size_t index = 0; index < orig.Properties().size(); ++index) {
    const auto& orig_key = orig_itr->first;
    const auto& dest_key = dest_itr->first;
    EXPECT_EQ(orig_key, dest_key);

    const auto& orig_prop = orig_itr->second;
    const auto& dest_prop = dest_itr->second;
    EXPECT_EQ(orig_prop.Key(), dest_prop.Key());
    EXPECT_EQ(orig_prop.Type(), dest_prop.Type());
    EXPECT_EQ(orig_prop.IsNull(), dest_prop.IsNull());
    if (orig_prop.Type() == MetricType::String) {
      EXPECT_EQ(orig_prop.Value<std::string>(), dest_prop.Value<std::string>());
    } else if (orig_prop.Type() == MetricType::Boolean) {
      EXPECT_EQ(orig_prop.Value<bool>(), dest_prop.Value<bool>());
    }
    ++orig_itr;
    ++dest_itr;
  }

  std::cout << dest.DebugString() << std::endl;
}

} // end namespace