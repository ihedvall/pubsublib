/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <workflow/itaskfactory.h>
#include <workflow/itask.h>

namespace pub_sub {

class PubSubWorkflowFactory : public workflow::ITaskFactory {
 public:
  PubSubWorkflowFactory();
  [[nodiscard]] std::unique_ptr<workflow::ITask> CreateTask(const workflow::ITask& source) const override;

};

} // pub_sub

