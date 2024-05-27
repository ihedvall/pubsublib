/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "sparkplug_b_c_sharp.pb.h"
#include "pubsub/ivalue.h"
#include "pubsub/ipayload.h"
namespace pub_sub {

class PayloadHelper {
 public:
  static void MetricToProtobuf(const IValue& source, org::eclipse::tahu::protobuf::Payload_Metric& dest);
  static void ProtobufToMetric(const org::eclipse::tahu::protobuf::Payload_Metric& source, IValue& dest);
  static void PayloadToProtobuf(const IPayload& source, org::eclipse::tahu::protobuf::Payload& dest);
};

} // pub_sub
