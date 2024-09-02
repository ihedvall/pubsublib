/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
/** \file
 * \brief Defines support functions that most Sparkplug B object uses.
 *
 */
#pragma once

#include <cstdint>

namespace pub_sub {

class SparkplugHelper {
 public:
  /** \brief Returns number of milliseconds since 1920 (UTV).
   *
   * Function that returns number of milliseconds since midnight 1 January 1970.
   * The timestamp is in UTC.
   *
   * @return
   */
  static uint64_t NowMs();

};

} // pub_sub


