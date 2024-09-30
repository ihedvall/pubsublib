/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include <string_view>

#include <gtest/gtest.h>
#include <util/timestamp.h>
#include "pubsub/itopic.h"
#include "pubsub/pubsubfactory.h"

using namespace util::time;

namespace pub_sub::test {

TEST(TestTopic, Properties ) {


  // Need to create a client to create a topic
  auto client = PubSubFactory::CreatePubSubClient(PubSubType::SparkplugNode);
  ASSERT_TRUE(client);
  {
    constexpr std::string_view kTopicName = "spBv1.0/GroupId/Message/NodeId/DeviceId";
    auto *topic = client->CreateTopic();
    ASSERT_TRUE(topic != nullptr);
    topic->Topic(kTopicName.data());
    EXPECT_STREQ(topic->Topic().c_str(), kTopicName.data());
    EXPECT_STREQ(topic->Namespace().c_str(), "spBv1.0");
    EXPECT_STREQ(topic->GroupId().c_str(), "GroupId");
    EXPECT_STREQ(topic->MessageType().c_str(), "Message");
    EXPECT_STREQ(topic->NodeId().c_str(), "NodeId");
    EXPECT_STREQ(topic->DeviceId().c_str(), "DeviceId");

    EXPECT_TRUE(topic->ContentType().empty());

    constexpr std::string_view kJsonType = "application/json";
    topic->ContentType(kJsonType.data());
    EXPECT_STREQ(topic->ContentType().c_str(), kJsonType.data());


    constexpr std::string_view kProtobufType = "application/protobuf";
    topic->ContentType(kProtobufType.data());
    EXPECT_STREQ(topic->ContentType().c_str(), kProtobufType.data());

    topic->Qos(QualityOfService::Qos2);
    EXPECT_EQ(topic->Qos(), QualityOfService::Qos2);

    EXPECT_FALSE(topic->Retained());
    topic->Retained(true);
    EXPECT_TRUE(topic->Retained());

    EXPECT_FALSE(topic->Publish());
    topic->Publish(true);
    EXPECT_TRUE(topic->Publish());
  }
  {
    constexpr std::string_view kTopicState = "spBv1.0/STATE/HostId";
    auto *topic = client->CreateTopic();
    ASSERT_TRUE(topic != nullptr);
    topic->Topic(kTopicState.data());
    EXPECT_STREQ(topic->Topic().c_str(), kTopicState.data());
    EXPECT_STREQ(topic->Namespace().c_str(), "spBv1.0");
    EXPECT_TRUE(topic->GroupId().empty());
    EXPECT_STREQ(topic->MessageType().c_str(), "STATE");
    EXPECT_STREQ(topic->NodeId().c_str(), "HostId");
    EXPECT_TRUE(topic->DeviceId().empty());

    auto timestamp = topic->CreateMetric("timestamp");
    ASSERT_TRUE(timestamp);
    EXPECT_STREQ(timestamp->Name().c_str(), "timestamp");


  }
  {
    constexpr std::string_view kTopicWild = "spBv1.0/#";
    auto *topic = client->CreateTopic();
    ASSERT_TRUE(topic != nullptr);
    topic->Topic(kTopicWild.data());
    EXPECT_STREQ(topic->Topic().c_str(), kTopicWild.data());
    EXPECT_STREQ(topic->Namespace().c_str(), "spBv1.0");
    EXPECT_TRUE(topic->IsWildcard());
  }
}

} // pub_sub::test

