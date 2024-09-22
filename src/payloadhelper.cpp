/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "payloadhelper.h"
#include "util/logstream.h"
#include "sparkplughelper.h"

using namespace org::eclipse::tahu::protobuf;
namespace {

pub_sub::MetricType ProtobufDataTypeToMetricType(uint32_t pb_type) {
  pub_sub::MetricType type = pub_sub::MetricType::Unknown;
  switch (pb_type) {
    case Int8:
      type = pub_sub::MetricType::Int8;
      break;

    case Int16:
      type = pub_sub::MetricType::Int16;
      break;

    case Int32:
      type = pub_sub::MetricType::Int32;
      break;

    case Int64:
      type = pub_sub::MetricType::Int64;
      break;

    case UInt8:
      type = pub_sub::MetricType::UInt8;
      break;

    case UInt16:
      type = pub_sub::MetricType::UInt16;
      break;

    case UInt32:
      type = pub_sub::MetricType::UInt32;
      break;

    case UInt64:
      type = pub_sub::MetricType::UInt64;
      break;

    case Float:
      type = pub_sub::MetricType::Float;
      break;

    case Double:
      type = pub_sub::MetricType::Double;
      break;

    case Boolean:
      type = pub_sub::MetricType::Boolean;
      break;

    case String:
      type = pub_sub::MetricType::String;
      break;

    case DateTime:
      type = pub_sub::MetricType::DateTime;
      break;

    case Text:
      type = pub_sub::MetricType::Text;
      break;

    case UUID:
      type = pub_sub::MetricType::UUID;
      break;

    case DataSet:
      type = pub_sub::MetricType::DataSet;
      break;

    case Bytes:
      type = pub_sub::MetricType::Bytes;
      break;

    case File:
      type = pub_sub::MetricType::File;
      break;

    case Template:
      type = pub_sub::MetricType::Template;
      break;

    case PropertySet:
      type = pub_sub::MetricType::PropertySet;
      break;

    case PropertySetList:
      type = pub_sub::MetricType::PropertySetList;
      break;

    case Int8Array:
      type = pub_sub::MetricType::Int8Array;
      break;

    case Int16Array:
      type = pub_sub::MetricType::Int16Array;
      break;

    case Int32Array:
      type = pub_sub::MetricType::Int32Array;
      break;

    case Int64Array:
      type = pub_sub::MetricType::Int64Array;
      break;

    case UInt8Array:
      type = pub_sub::MetricType::UInt8Array;
      break;

    case UInt16Array:
      type = pub_sub::MetricType::UInt16Array;
      break;

    case UInt32Array:
      type = pub_sub::MetricType::UInt32Array;
      break;

    case UInt64Array:
      type = pub_sub::MetricType::UInt64Array;
      break;

    case FloatArray:
      type = pub_sub::MetricType::FloatArray;
      break;

    case DoubleArray:
      type = pub_sub::MetricType::DoubleArray;
      break;

    case BooleanArray:
      type = pub_sub::MetricType::BooleanArray;
      break;

    case StringArray:
      type = pub_sub::MetricType::StringArray;
      break;

    case DateTimeArray:
      type = pub_sub::MetricType::DateTimeArray;
      break;

    default:
      break;
  }
  return type;
}

}

namespace pub_sub {
PayloadHelper::PayloadHelper(Payload &source)
    : source_(source) {
}

void PayloadHelper::WriteProtobuf() {
  // Note that dst payload is the protobuf payload not Payload.
  // Source is the Payload and at the end the protobuf dest shall
  // be serialized to the source body (data bytes).
  try {
    org::eclipse::tahu::protobuf::Payload pb_payload; // Note not a Payload is a protobuf payload
    pb_payload.set_timestamp(source_.Timestamp());
    auto seq_no = source_.SequenceNumber();
    pb_payload.set_seq(seq_no);
    ++seq_no;
    if (seq_no > 255) {
      seq_no = 0;
    }
    source_.SequenceNumber(seq_no);
    if (!source_.Uuid().empty()) {
      pb_payload.set_uuid(source_.Uuid());
    }

    // METRIC LIST
    const auto &metric_list = source_.Metrics();
    for (const auto &[name, metric] : metric_list) {
      if (name.empty()) {
        continue;
      }
      // If the create_all_metrics_ is false, only updated metric values should
      // be reported. The NBIRTH amd DBIRTH messages include all metric data, properties and
      // metadata.
      const bool include_metric = WriteAllMetrics() || metric->IsUpdated();
      if (include_metric) {
        auto *met = pb_payload.add_metrics();
        if (met == nullptr) {
          continue;
        }
        WriteMetric(*metric, *met);
      }
    }

    // Serialize the protobuf to the IPayloads body (data bytes)
    const auto data_size = pb_payload.ByteSizeLong();
    auto &body = source_.Body();
    if (data_size == 0) {
      body.clear();
      return;
    }
    body.resize(data_size);
    const bool serialize = pb_payload.SerializeToArray(body.data(), static_cast<int>(body.size()));
    if (!serialize) {
      LOG_ERROR() << "Failed to serialize to protobuf.";
      body.clear();
    }
  } catch (const std::exception &err) {
    LOG_ERROR() << "Protobuf Serialization Error: " << err.what();
  }
}

void PayloadHelper::WriteMetric(const Metric &metric, Payload_Metric &dest) const {
  try {
    dest.set_name(metric.Name()); // Include name for tracing purpose
    dest.set_alias(metric.Alias());
    dest.set_timestamp(metric.Timestamp());
    dest.set_is_historical(metric.IsHistorical());
    dest.set_is_transient(metric.IsTransient());
    dest.set_is_null(metric.IsNull());

    // The above is the only parameters to send
    if (!WriteAllMetrics()) {
      return;
    }
    dest.set_datatype(static_cast<uint32_t>(metric.Type()));
    switch (metric.Type()) {
      case MetricType::Int8:
      case MetricType::Int16:
      case MetricType::Int32:
        dest.set_int_value(static_cast<uint32_t>(metric.Value<int32_t>()));
        break;

      case MetricType::Int64:
        dest.set_long_value(static_cast<uint64_t>(metric.Value<int64_t>()));
        break;

      case MetricType::UInt8:
      case MetricType::UInt16:
      case MetricType::UInt32:
        dest.set_int_value(metric.Value<uint32_t>());
        break;

      case MetricType::UInt64:
        dest.set_long_value(metric.Value<uint64_t>());
        break;

      case MetricType::Float:
        dest.set_float_value(metric.Value<float>());
        break;

      case MetricType::Double:
        dest.set_double_value(metric.Value<double>());
        break;

      case MetricType::Boolean:
        dest.set_boolean_value(metric.Value<bool>());
        break;

      case MetricType::String:
      case MetricType::Unknown:
      default:
        dest.set_string_value(metric.Value<std::string>());
        break;
    }

    const auto &property_list = metric.Properties();
    if (!property_list.empty()) {
      auto *pb_property_set = new Payload_PropertySet;
      WritePropertySet(property_list, *pb_property_set);
      dest.set_allocated_properties(pb_property_set);
    }
  } catch (const std::exception& err) {
    LOG_ERROR() << "Failed to write metric. Error: " << err.what();
  }
}

void PayloadHelper::WritePropertySet(const MetricPropertyList &property_list, Payload_PropertySet &pb_property_set) {
  try {
    for (const auto &[name, prop] : property_list) {
      if (name.empty()) {
        continue;
      }
      pb_property_set.add_keys(name);
      auto *pb_property_value = pb_property_set.add_values();
      if (pb_property_value == nullptr) {
        throw std::runtime_error("Failed to create a property value");
      }
      pb_property_value->set_type(static_cast<DataType>(prop.Type()));
      pb_property_value->set_is_null(prop.IsNull());

      switch (prop.Type()) {
        case MetricType::Int8:
        case MetricType::Int16:
        case MetricType::Int32:
          pb_property_value->set_int_value(static_cast<uint32_t>( prop.Value<int32_t>() ));
          break;

        case MetricType::Int64:
          pb_property_value->set_long_value(static_cast<uint64_t>( prop.Value<int64_t>() ));
          break;

        case MetricType::UInt8:
        case MetricType::UInt16:
        case MetricType::UInt32:
          pb_property_value->set_int_value(prop.Value<uint32_t>());
          break;

        case MetricType::UInt64:
        case MetricType::DateTime:
          pb_property_value->set_long_value(prop.Value<uint64_t>());
          break;

        case MetricType::Float:
          pb_property_value->set_float_value(prop.Value<float>());
          break;

        case MetricType::Double:
          pb_property_value->set_double_value( prop.Value<double>() );
          break;

        case MetricType::Boolean:
          pb_property_value->set_boolean_value( prop.Value<bool>() );
          break;

        case MetricType::String:
        case MetricType::Unknown:
        default:
          pb_property_value->set_string_value(prop.Value<std::string>());
          break;
      }
    }

  } catch (const std::exception& err) {
    LOG_ERROR() << "Failed to write property set. Error: " << err.what();
  }

}
void PayloadHelper::ParseProtobuf() {
  // The Payload body (data bytes) should hold the protobuf data
  try {
    const auto &body = source_.Body();
    if (body.empty()) {
      return;
    }
    org::eclipse::tahu::protobuf::Payload pb_payload;
    bool parse = pb_payload.ParseFromArray(body.data(), static_cast<int>(body.size()));
    if (!parse) {
      throw std::runtime_error("Parsing error.");
    }
    // We need to handle if the pb_payload.bytes is in use. Strange design ?
    if (pb_payload.has_body()) {
      // Restart the parser
      const auto &body_list = pb_payload.body();
      pb_payload.Clear();
      parse = pb_payload.ParseFromArray(body_list.data(), static_cast<int>(body_list.size()));
      if (!parse) {
        throw std::runtime_error("Parsing error (pb_payload.body).");
      }
    }

    source_.Timestamp(pb_payload.has_timestamp() ? pb_payload.timestamp() : SparkplugHelper::NowMs());
    if (pb_payload.has_seq()) {
      source_.SequenceNumber(pb_payload.seq());
    }
    if (pb_payload.has_uuid()) {
      source_.Uuid(pb_payload.uuid());
    }
    // Read in the metrics
    for (const auto &pb_metric : pb_payload.metrics()) {
      std::shared_ptr<Metric> metric;
      std::string name;
      if (pb_metric.has_name()) {
        name = pb_metric.name();
        metric = source_.GetMetric(name);
      } else if (pb_metric.has_alias()) {
        metric = source_.GetMetric(pb_metric.alias());
      }
      // It's always possible to create metrics, but you cannot change
      // name, alias and data type on existing metrics.
      if (!metric && !name.empty()) {
        metric = source_.CreateMetric(name);
        if (!metric) {
          throw std::runtime_error("Create failed. Internal error");
        }
        if (pb_metric.has_alias()) {
          metric->Alias(pb_metric.alias());
        }
        if (pb_metric.has_datatype()) {
          metric->Type(ProtobufDataTypeToMetricType(pb_metric.datatype()));
        }
      }
      if (metric == nullptr) {
        continue; // Name cannot add the metrics
      }
      ParseMetric(pb_metric, *metric);
    }

  } catch (const std::exception &err) {
    LOG_ERROR() << "Protobuf parsing error. Error: " << err.what();
  }
}

void PayloadHelper::ParseMetric(const Payload_Metric &pb_metric, Metric &metric) {
  try {
    if (metric.Name().empty() && pb_metric.has_name()) {
      metric.Name(pb_metric.name());
    }
    if (metric.Alias() == 0 && pb_metric.has_alias() != 0) {
      metric.Alias(pb_metric.alias());
    }
    if (metric.Type() == MetricType::Unknown && pb_metric.has_datatype() != 0) {
      metric.Type(ProtobufDataTypeToMetricType(pb_metric.datatype()));
    }
    metric.Timestamp(pb_metric.has_timestamp() ? pb_metric.timestamp() : source_.Timestamp());
    metric.IsHistorical(pb_metric.has_is_historical() ? pb_metric.is_historical() : false);
    metric.IsTransient(pb_metric.has_is_historical() ? pb_metric.is_transient() : false);
    metric.IsNull(pb_metric.has_is_null() ? pb_metric.is_null() : false);

    if (pb_metric.has_int_value()) {
      switch (metric.Type()) {
        case MetricType::Int8:
        case MetricType::Int16:
        case MetricType::Int32:
        case MetricType::Int64:metric.Value(static_cast<int32_t>(pb_metric.int_value()));
          break;

        default:metric.Value(pb_metric.int_value());
          break;
      }
    } else if (pb_metric.has_long_value()) {
      switch (metric.Type()) {
        case MetricType::Int8:
        case MetricType::Int16:
        case MetricType::Int32:
        case MetricType::Int64:metric.Value(static_cast<int64_t>(pb_metric.long_value()));
          break;

        default:metric.Value(pb_metric.long_value());
          break;
      }
    } else if (pb_metric.has_float_value()) {
      metric.Value(pb_metric.float_value());
    } else if (pb_metric.has_double_value()) {
      metric.Value(pb_metric.double_value());
    } else if (pb_metric.has_boolean_value()) {
      metric.Value(pb_metric.boolean_value());
    } else if (pb_metric.has_string_value()) {
      metric.Value(pb_metric.string_value());
    } else if (pb_metric.has_bytes_value()) {
      metric.Value(pb_metric.bytes_value());
    }

    if (pb_metric.has_metadata()) {
      auto *meta_data = metric.CreateMetaData();
      if (meta_data != nullptr) {
        ParseMetaData(pb_metric.metadata(), *meta_data);
      }
    }

    if (pb_metric.has_properties()) {
      const auto &pb_prop_list = pb_metric.properties();
      for (int key = 0; key < pb_prop_list.keys_size(); ++key) {
        const auto &prop_key = pb_prop_list.keys(key);
        const auto value_list_size = pb_prop_list.values_size();
        if (prop_key.empty() || key >= value_list_size) {
          continue;
        }
        const auto &pb_property_value = pb_prop_list.values(key);

        MetricProperty *property = metric.GetProperty(prop_key);
        if (property == nullptr) {
          property = metric.CreateProperty(prop_key);
          if (property == nullptr) {
            throw std::runtime_error("Failed to create metric property");
          }
          if (pb_property_value.has_type()) {
            property->Type( ProtobufDataTypeToMetricType(pb_property_value.type()) );
          }
        }
        ParsePropertyValue(pb_property_value, *property);
      }
    }
  } catch (const std::exception& err) {
    LOG_ERROR() << "Parsing of metric failed. Error: " << err.what();
  }
}

void PayloadHelper::ParseMetaData(const Payload_MetaData &pb_meta_data, MetricMetadata &meta_data) {
  try {
    if (pb_meta_data.has_is_multi_part()) {
      meta_data.IsMultiPart( pb_meta_data.is_multi_part());
    }
    if (pb_meta_data.has_content_type()) {
      meta_data.ContentType( pb_meta_data.content_type());
    }
    if (pb_meta_data.has_size()) {
      meta_data.Size( pb_meta_data.size() );
    }
    if (pb_meta_data.has_seq()) {
      meta_data.SequenceNumber( pb_meta_data.seq() );
    }
    if (pb_meta_data.has_file_name()) {
      meta_data.FileName(pb_meta_data.file_name());
    }
    if (pb_meta_data.has_file_type()) {
      meta_data.FileType( pb_meta_data.file_type() );
    }
    if (pb_meta_data.has_md5()) {
      meta_data.Md5( pb_meta_data.md5() );
    }
    if (pb_meta_data.has_description()) {
      meta_data.Description( pb_meta_data.description() );
    }
  } catch (std::exception &err) {
    LOG_ERROR() << "Parsing meta data error. Error: " << err.what();
  }
}

void PayloadHelper::ParsePropertyValue(const Payload_PropertyValue &pb_property_value, MetricProperty &property) {
  try {
    property.IsNull( pb_property_value.has_is_null() ? pb_property_value.is_null() : false);
    if (pb_property_value.has_int_value()) {
      switch (property.Type()) {
        case MetricType::Int8:
        case MetricType::Int16:
        case MetricType::Int32:
        case MetricType::Int64:
        case MetricType::Float:
        case MetricType::Double:
          property.Value( static_cast<int32_t>(pb_property_value.int_value()) );
          break;

        default:
          property.Value(pb_property_value.int_value());
          break;
      }
    } else if (pb_property_value.has_long_value()) {
      switch (property.Type()) {
        case MetricType::Int8:
        case MetricType::Int16:
        case MetricType::Int32:
        case MetricType::Int64:
        case MetricType::Float:
        case MetricType::Double:
          property.Value( static_cast<int64_t>(pb_property_value.long_value()) );
          break;

        default:
          property.Value( pb_property_value.long_value() );
          break;
      }
    } else if (pb_property_value.has_float_value()) {
      property.Value( pb_property_value.float_value() );
    } else if (pb_property_value.has_double_value()) {
      property.Value( pb_property_value.double_value() );
    } else if (pb_property_value.has_boolean_value()) {
      property.Value( pb_property_value.boolean_value() );
    } else if (pb_property_value.has_string_value()) {
      property.Value( pb_property_value.string_value() );
    } else if (pb_property_value.has_propertyset_value()) {
      const auto &pb_property_set = pb_property_value.propertyset_value();
      if (property.PropertyArray().empty()) {
        property.Value("");
        property.PropertyArray().resize(1, {});
      }
      auto &prop_map = property.PropertyArray()[0];
      ParsePropertySet(pb_property_set, prop_map);
    }
  } catch (const std::exception& err) {
    LOG_ERROR() << "Parsing of property value failed. Error: " << err.what();
  }
}

void PayloadHelper::ParsePropertySet(const Payload_PropertySet &pb_property_set, MetricPropertyList &property_list) {
  try {
    for (int sub = 0; sub < pb_property_set.keys_size(); ++sub) {
      const auto &sub_key = pb_property_set.keys(sub);
      if (sub_key.empty() || sub >= pb_property_set.values_size()) {
        continue;
      }
      const auto &sub_value = pb_property_set.values(sub);
      auto sub_exist = property_list.find(sub_key);
      if (sub_exist == property_list.end()) {
        MetricProperty sub_prop;
        sub_prop.Key( sub_key);
        ParsePropertyValue(sub_value, sub_prop);
        property_list.emplace(sub_key, sub_prop);
      } else {
        ParsePropertyValue(sub_value, sub_exist->second);
      }
    }
  } catch (const std::exception& err) {
    LOG_ERROR() << "Parsing of property set failed. Error: " << err.what();
  }
}


}// pub_sub