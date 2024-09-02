/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "sparkplughelper.h"

#include <chrono>

using SystemClock = std::chrono::system_clock;
using TimeStamp = std::chrono::time_point<SystemClock>;
namespace pub_sub {

uint64_t SparkplugHelper::NowMs() {
  const TimeStamp timestamp = SystemClock::now();
  // Just in case epoch not is 1970
  const auto ms_midnight = std::chrono::duration_cast<std::chrono::milliseconds>(
      timestamp.time_since_epoch()) % 1000;
  const auto sec_1970 = SystemClock::to_time_t(timestamp);
  uint64_t ms = sec_1970;
  ms *= 1'000;
  ms += ms_midnight.count();
  return ms;
}

} // pub_sub