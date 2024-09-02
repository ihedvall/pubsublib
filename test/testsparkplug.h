/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <util/utilfactory.h>

namespace pub_sub::test {

class TestSparkplug : public testing::Test {
 public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

 protected:

};

} // pub_sub::test


