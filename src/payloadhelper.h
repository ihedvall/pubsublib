/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "sparkplug_b.pb.h"
#include "pubsub/imetric.h"
#include "pubsub/ipayload.h"
namespace pub_sub {

class PayloadHelper {
 public:
  static void MetricToProtobuf(const IMetric& source, org::eclipse::tahu::protobuf::Payload_Metric& dest);
  static void ProtobufToMetric(const org::eclipse::tahu::protobuf::Payload_Metric& source,  IMetric& dest);
  static void PayloadToProtobuf(const IPayload& source, org::eclipse::tahu::protobuf::Payload& dest);
};

} // pub_sub
