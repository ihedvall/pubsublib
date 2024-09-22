/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace pub_sub {

class MetricMetadata {
 public:
  void IsMultiPart(bool multi_part) { is_multi_part_ = multi_part; }
  [[nodiscard]] bool IsMultiPart() const { return is_multi_part_; }

  void ContentType(const std::string& content) { content_type_ = content; }
  [[nodiscard]] const std::string& ContentType() const { return content_type_; }

  void Size(uint64_t size) { size_ = size; }
  [[nodiscard]] uint64_t Size() const { return size_; }

  void SequenceNumber(uint64_t seq) { seq_ = seq; }
  [[nodiscard]] uint64_t SequenceNumber() const { return seq_; }

  void FileName(const std::string& file_name) { file_name_ = file_name; }
  [[nodiscard]] const std::string& FileName() const { return file_name_; }

  void FileType(const std::string& file_type) { file_type_ = file_type; }
  [[nodiscard]] const std::string& FileType() const { return file_type_; }

  void Md5(const std::string& md5) { md5_ = md5; }
  [[nodiscard]] const std::string& Md5() const { return md5_; }

  void Description(const std::string& description) { description_ = description; }
  [[nodiscard]] const std::string& Description() const { return description_; }
 private:
  bool is_multi_part_ = false; ///< True if this is a multi part message
  std::string content_type_;   ///< Content media type string
  uint64_t size_ = 0;          ///< Number of bytes
  uint64_t seq_ = 0;           ///< Multi-part sequence number
  std::string file_name_;      ///< File name
  std::string file_type_;      ///< File type as xml or json
  std::string md5_;            ///< MD5 checksum as string
  std::string description_;    ///< Description
};

} // pub_sub


