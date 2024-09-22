//
// Created by ihedv on 2024-09-22.
//

#include <utility>

#include "pubsub/metricproperty.h"

namespace pub_sub {

MetricProperty::MetricProperty(const MetricProperty &property)
: key_(property.key_),
  type_(property.type_),
  is_null_(property.is_null_ ),
  value_(property.value_) {

}
MetricProperty &MetricProperty::operator=(const MetricProperty &property) {
  if (&property == this) {
    return *this;
  }
  key_ = property.key_;
  type_ = property.type_;
  is_null_ = property.is_null_;
  value_ = property.value_;
  return *this;
}
void MetricProperty::IsNull(bool is_null) {
  std::scoped_lock lock(property_mutex_);
  is_null_ = is_null;
}

bool MetricProperty::IsNull() const {
  std::scoped_lock lock(property_mutex_);
  return is_null_;
}

MetricProperty::MetricProperty(std::string key, std::string value)
: key_(std::move(key)),
  type_(MetricType::String),
  is_null_(false),
  value_(std::move(value)) {

}

std::vector<MetricPropertyList> &MetricProperty::PropertyArray() {
  return prop_array_;
}

const std::vector<MetricPropertyList> &MetricProperty::PropertyArray() const {
  return prop_array_;
}

template<>
bool MetricProperty::Value() const {
  std::scoped_lock lock(property_mutex_);
  if (value_.empty()) {
    return false;
  }
  switch (value_[0]) {
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

template<>
uint8_t MetricProperty::Value() const {
  std::scoped_lock lock(property_mutex_);
  uint8_t temp = 0;
  try {
    temp = static_cast< uint8_t>(std::stoul(value_));
  } catch (const std::exception&) {}
  return temp;
}

template<>
int8_t MetricProperty::Value() const {
  std::scoped_lock lock(property_mutex_);
  int8_t temp = 0;
  try {
    temp = static_cast< int8_t>(std::stoi(value_));
  } catch (const std::exception&) {

  }
  return temp;
}

template<>
std::string MetricProperty::Value() const {
  std::scoped_lock lock(property_mutex_);
  return value_;
}

template<>
void MetricProperty::Value(std::string value) {
  std::scoped_lock lock(property_mutex_);
  value_ = std::move(value);
}

template<>
void MetricProperty::Value(const char* value) {
  std::scoped_lock lock(property_mutex_);
  value_ = value != nullptr ? value : "";
}

template<>
void MetricProperty::Value(bool value) {
  std::scoped_lock lock(property_mutex_);
  value_ = value ? "1" : "0";
}

} // pub_sub