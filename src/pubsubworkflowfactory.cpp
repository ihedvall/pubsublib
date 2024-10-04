/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsubworkflowfactory.h"

using namespace workflow;

namespace pub_sub {

PubSubWorkflowFactory::PubSubWorkflowFactory()
: ITaskFactory() {
  name_ = "Pub/Sub Factory";
  description_ = "Public/Subscribe workflow task templates.";

  // Todo: Create a list of templates


}
std::unique_ptr<workflow::ITask> PubSubWorkflowFactory::CreateRunner(const workflow::ITask &source) const {
  std::unique_ptr<ITask> task;
  const auto& template_name = source.Template();
  return task;
}

} // pub_sub