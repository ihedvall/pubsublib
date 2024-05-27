/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <gtest/gtest.h>
#include <memory>
#include <util/utilfactory.h>

namespace pub_sub::test {

class TestMqtt : public testing::Test {
 public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

 protected:
  static std::string broker_;
  static std::string broker_name_;
  static std::unique_ptr<util::log::IListen> listen_;

};

} // pub_sub::test

