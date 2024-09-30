/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <workflow/irunnerfactory.h>
#include <workflow/irunner.h>

namespace pub_sub {

class PubSubWorkflowFactory : public workflow::IRunnerFactory {
 public:
  PubSubWorkflowFactory();
  [[nodiscard]] std::unique_ptr<workflow::IRunner> CreateRunner(const workflow::IRunner& source) const override;

};

} // pub_sub

