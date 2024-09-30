/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "sparkplug_b.pb.h"
#include "pubsub/metric.h"
#include "pubsub/payload.h"

namespace pub_sub {

class PayloadHelper final {
 public:
  PayloadHelper() = delete;
  explicit PayloadHelper(Payload& source);

  void WriteAllMetrics(bool write_all) { write_all_metrics_ = write_all; }
  [[nodiscard]] bool WriteAllMetrics() const { return write_all_metrics_; }

  void CreateMetrics(bool create) { create_metrics_ = create; }
  [[nodiscard]] bool CreateMetrics() const { return create_metrics_; }

  void WriteProtobuf();

  void WriteMetric(const Metric& metric,
                          org::eclipse::tahu::protobuf::Payload_Metric& pb_metric) const;
  bool WritePropertySet(const MetricPropertyList& property_list,
                   org::eclipse::tahu::protobuf::Payload_PropertySet& pb_property_set) const;

  void ParseProtobuf();
  [[nodiscard]] std::string DebugProtobuf() const;

  void ParseMetric(const org::eclipse::tahu::protobuf::Payload_Metric& pb_metric, Metric& metric);

  static void ParseMetaData(const org::eclipse::tahu::protobuf::Payload_MetaData& pb_meta_data,
                            MetricMetadata& meta_data);

  void ParsePropertyValue(const org::eclipse::tahu::protobuf::Payload_PropertyValue& pb_property_value,
                            MetricProperty& property);

  void ParsePropertySet(const org::eclipse::tahu::protobuf::Payload_PropertySet& pb_property_set,
                              MetricPropertyList& property_list);
 private:
   Payload& source_;
   bool write_all_metrics_ = false;
   bool create_metrics_ = false;

};

} // pub_sub
