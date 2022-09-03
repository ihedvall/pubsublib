/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "payloadhelper.h"

using namespace org::eclipse::tahu::protobuf;
namespace {

template <typename T>
T PropertyValue(const std::string& value) {
  T temp = {};
  try {
    std::istringstream str(value);
    str >> temp;
  } catch (const std::exception& ) {
  }
  return temp;
}

template<>
std::string PropertyValue(const std::string& value) {
  return value;
}

template<>
int8_t PropertyValue(const std::string& value) {
  int8_t temp = 0;
  try {
    temp = static_cast< int8_t>(std::stoi(value));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
uint8_t PropertyValue(const std::string& value) {
  uint8_t temp = 0;
  try {
    temp = static_cast< uint8_t>(std::stoul(value));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
bool PropertyValue(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  switch (value[0]) {
    case 'Y':
    case 'y':
    case 'T':
    case 't':
    case '1':
      return true;

    default:
      break;
  }
  return false;
}

}

namespace pub_sub {
void PayloadHelper::MetricToProtobuf(const IMetric &source, Payload_Metric &dest) {
  dest.set_name(source.Name());
  dest.set_alias(source.Alias());
  dest.set_timestamp(source.Timestamp());
  dest.set_datatype(static_cast<uint32_t>(source.Type()));
  dest.set_is_historical(source.IsHistorical());
  dest.set_is_transient(source.IsTransient());
  dest.set_is_null(source.IsNull());


  switch (source.Type()) {
    case ValueType::Int8:
    case ValueType::Int16:
    case ValueType::Int32:
      dest.set_int_value(static_cast<uint32_t>(source.Value<int32_t>()));
      break;

    case ValueType::Int64:
      dest.set_long_value(static_cast<uint64_t>(source.Value<int64_t>()));
      break;

    case ValueType::UInt8:
    case ValueType::UInt16:
    case ValueType::UInt32:
      dest.set_int_value(source.Value<uint32_t>());
      break;

    case ValueType::UInt64:
    case ValueType::DateTime:
      dest.set_long_value(source.Value<uint64_t>());
      break;

    case ValueType::Float:
      dest.set_float_value(source.Value<float>());
      break;

    case ValueType::Double:
      dest.set_double_value(source.Value<double>());
      break;

    case ValueType::Boolean:
      dest.set_boolean_value(source.Value<bool>());
      break;

    case ValueType::Unknown:
    default:
      dest.set_string_value(source.Value<std::string>());
      break;
  }
  const auto& prop_list = source.Properties();
  if (!prop_list.empty()) {
    auto *props = new Payload_PropertySet;
    for (const auto &prop: prop_list) {
      if (prop.key.empty()) {
        continue;
      }
      props->add_keys(prop.key);
      auto* prop_values = props->add_values();
      prop_values->set_type(static_cast<DataType>(prop.type));

      switch (prop.type) {
        case ValueType::Int8:
        case ValueType::Int16:
        case ValueType::Int32:
          prop_values->set_int_value(static_cast<uint32_t>(PropertyValue<int32_t>(prop.value)));
          break;

        case ValueType::Int64:
          prop_values->set_long_value(static_cast<uint64_t>(PropertyValue<int64_t>(prop.value)));
          break;

        case ValueType::UInt8:
        case ValueType::UInt16:
        case ValueType::UInt32:
          prop_values->set_int_value(PropertyValue<uint32_t>(prop.value));
          break;

        case ValueType::UInt64:
        case ValueType::DateTime:
          prop_values->set_long_value(PropertyValue<uint64_t>(prop.value));
          break;

        case ValueType::Float:
          prop_values->set_float_value(PropertyValue<float>(prop.value));
          break;

        case ValueType::Double:
          prop_values->set_double_value(PropertyValue<double>(prop.value));
          break;

        case ValueType::Boolean:
          prop_values->set_boolean_value(PropertyValue<bool>(prop.value));
          break;

        case ValueType::Unknown:
        default:
          prop_values->set_string_value(prop.value);
          break;
      }
    }
    dest.set_allocated_properties(props);
  }
}

void PayloadHelper::ProtobufToMetric(const Payload_Metric &source, IMetric &dest) {
  if (source.has_name()) {
    dest.Name(source.name());
  }
  if (source.has_alias()) {
    dest.Alias(source.alias());
  }

  if (source.has_timestamp()) {
    dest.Timestamp(source.timestamp());
  }
  if (source.has_datatype()) {
    dest.Type(static_cast<ValueType>(source.datatype()));
  }
  if (source.has_is_historical()) {
    dest.IsHistorical(source.is_historical());
  }
  if (source.has_is_transient()) {
    dest.IsTransient(source.is_transient());
  }
  if (source.has_is_null()) {
    dest.IsNull(source.is_null());
  }

  if (source.has_int_value()) {
    switch (dest.Type()) {
      case ValueType::Int8:
      case ValueType::Int16:
      case ValueType::Int32:
      case ValueType::Int64:
        dest.Value(static_cast<int32_t>(source.int_value()));
        break;

      default:
        dest.Value(source.int_value());
        break;
    }
  } else if (source.has_long_value()) {
    switch (dest.Type()) {
      case ValueType::Int8:
      case ValueType::Int16:
      case ValueType::Int32:
      case ValueType::Int64:
        dest.Value(static_cast<int64_t>(source.long_value()));
        break;

      default:
        dest.Value(source.long_value());
        break;
    }
  } else if (source.has_float_value()) {
    dest.Value(source.float_value());
  } else if (source.has_double_value()) {
    dest.Value(source.double_value());
  } if (source.has_boolean_value()) {
    dest.Value(source.boolean_value());
  } else if (source.has_string_value()) {
    dest.Value(source.string_value());
  }


  // Block all changes on properties
  const auto& prop_list = dest.Properties();
  if (source.has_properties() && prop_list.empty()) {
    const auto& props = source.properties();
    for (int key = 0; key < props.keys_size(); ++key) {
      const auto& prop_key = props.keys(key);
      if (prop_key.empty() || key >= props.values_size()) {
        continue;
      }
      const auto& prop_value = props.values(key);
      Property new_prop;
      new_prop.key = prop_key;
      new_prop.type = prop_value.has_type() ? static_cast<ValueType>(prop_value.type()) : ValueType::String;
      new_prop.is_null = prop_value.has_is_null() && prop_value.is_null();
      if (prop_value.has_int_value()) {
        switch (new_prop.type) {
          case ValueType::Int8:
          case ValueType::Int16:
          case ValueType::Int32:
          case ValueType::Int64:
            new_prop.value = std::to_string(static_cast<int32_t>(prop_value.int_value()));
            break;

          default:
            new_prop.value = std::to_string(prop_value.int_value());
            break;
        }
      } else if (prop_value.has_long_value()) {
        switch (new_prop.type) {
          case ValueType::Int8:
          case ValueType::Int16:
          case ValueType::Int32:
          case ValueType::Int64:
            new_prop.value = std::to_string(static_cast<int64_t>(prop_value.long_value()));
            break;

          default:
            new_prop.value = std::to_string(prop_value.long_value());
            break;
        }
      } else if (prop_value.has_float_value()) {
        new_prop.value = util::string::FloatToString(prop_value.float_value());
      } else if (prop_value.has_double_value()) {
        new_prop.value = util::string::DoubleToString(prop_value.double_value());
      } if (prop_value.has_boolean_value()) {
        new_prop.value = prop_value.boolean_value() ? "true" : "false";
      } else if (prop_value.has_string_value()) {
        new_prop.value = prop_value.string_value();
      }
      dest.AddProperty(new_prop);
    }

  }
}

void PayloadHelper::PayloadToProtobuf(const IPayload &source, Payload &dest) {
  dest.set_timestamp(source.Timestamp());
  auto seq_no = source.SequenceNumber();
  dest.set_seq(seq_no);
  ++seq_no;
  if (seq_no > 255) {
    seq_no = 0;
  }
  source.SequenceNumber(seq_no);
  if (!source.Uuid().empty()) {
    dest.set_uuid(source.Uuid());
  }
  const auto& metric_list = source.Metrics();
  for (const auto& metric : metric_list) {
    auto* met = dest.add_metrics();
    MetricToProtobuf(*metric.second,*met);
  }

}

} // pub_sub