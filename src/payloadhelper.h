/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "sparkplug_b.pb.h"
#include "pubsub/imetric.h"
#include "pubsub/ipayload.h"

namespace pub_sub {

class PayloadHelper final {
 public:
  PayloadHelper() = delete;
  explicit PayloadHelper(IPayload& source);

  void WriteAllMetrics(bool write_all) { write_all_metrics_ = write_all; }
  [[nodiscard]] bool WriteAllMetrics() const { return write_all_metrics_; }

  void WriteProtobuf();

  void WriteMetric(const IMetric& metric,
                          org::eclipse::tahu::protobuf::Payload_Metric& pb_metric);
  void WritePropertySet(const MetricPropertyList& property_list,
                   org::eclipse::tahu::protobuf::Payload_PropertySet& pb_property_set);

  void ParseProtobuf();

  void ParseMetric(const org::eclipse::tahu::protobuf::Payload_Metric& pb_metric, IMetric& metric);

  static void ParseMetaData(const org::eclipse::tahu::protobuf::Payload_MetaData& pb_meta_data,
                            MetricMetaData& meta_data);

  static void ParsePropertyValue(const org::eclipse::tahu::protobuf::Payload_PropertyValue& pb_property_value,
                            MetricProperty& property);

  static void ParsePropertySet(const org::eclipse::tahu::protobuf::Payload_PropertySet& pb_property_set,
                              MetricPropertyList& property_list);
 private:
   IPayload& source_;
   bool write_all_metrics_ = false;

};

} // pub_sub
